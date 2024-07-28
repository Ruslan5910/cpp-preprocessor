#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintError(const path& p, const string& s, int n) {
    cout << "unknown include file "s << p.string() << " at file "s << s << " at line "s << n << endl;
}

vector<path>::const_iterator FindInDirectories(const vector<path>& include_directories, const path& p) {
    return find_if(include_directories.begin(), include_directories.end(), [p](path incl) {
                                                                               return exists(incl / p);
                                                                            });
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in.is_open()) {
        return false;
    }
    
    ofstream out(out_file, ios::out | ios::app);
    
    static regex custom_file(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex std_file(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    
    smatch m;
    string line;
    int line_number = 0;
    while (getline(in, line)) {
        ++line_number;
        if (regex_match(line, m, custom_file)) {
            vector<path>::const_iterator result;
            if (exists(in_file.parent_path() / path(m[1]))) {
                Preprocess(in_file.parent_path() / path(m[1]), out_file, include_directories);
            } else if (result = FindInDirectories(include_directories, path(m[1])); result != include_directories.end()) {
                Preprocess(*result / path(m[1]), out_file, include_directories);
            } else {
                PrintError(path(m[1]), in_file.string(), line_number);
                return false;
            }
        } else if (regex_match(line, m, std_file)) {
            vector<path>::const_iterator result = FindInDirectories(include_directories, path(m[1]));
            if (result != include_directories.end()) {
                Preprocess(*result / path(m[1]), out_file, include_directories);
            } else {
                PrintError(path(m[1]), in_file.string(), line_number);
                return false;
            }
        } else {
            out << line << endl;
        }
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);
    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;
    
    cout << GetFileContents("sources/a.in"s) << endl;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
