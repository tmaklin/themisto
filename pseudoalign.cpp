#include "KallistoLite.hh"
#include "input_reading.hh"
#include <string>
#include <cstring>

using namespace std;

struct Config{
    vector<string> query_files;
    vector<string> outfiles;
    string index_dir;
    string temp_dir;
    LL memory_megas = 1000;
    
    bool reverse_complements = false;
    LL n_threads = 1;

    void check_valid(){
        for(string query_file : query_files){
            if(query_file != ""){
                check_readable(query_file);
            }
        }

        for(string outfile : outfiles){
            assert(outfile != "");
            check_writable(outfile);
        }

        assert(query_files.size() == outfiles.size());
        
        assert(index_dir != "");
        check_dir_exists(index_dir);

        assert(temp_dir != "");
        check_dir_exists(temp_dir);
    }
};

char get_rc(char c){
    switch(c){
        case 'A': return 'T';
        case 'T': return 'A';
        case 'C': return 'G';
        case 'G': return 'C';
        default: return c;
    }
}

string get_rc(string S){
    string rc;
    for(LL i = S.size()-1; i >= 0; i--){
        rc += get_rc(S[i]);
    }
    return rc;
}

vector<string> read_lines(string filename){
    check_readable(filename);
    vector<string> lines;
    ifstream in(filename);
    string line;
    while(getline(in, line)){
        lines.push_back(line);
    }
    return lines;
}

int main(int argc, char** argv){

    if(argc == 1){
        cerr << "The query can be given as one file, or as a file with a list of files." << endl;
        cerr << "To give a single query file, use the following two options: " << endl;
        cerr << "  --query-file [filename]" << endl;
        cerr << "  --outfile [path] (directory must exist before running)" << endl;
        cerr << "To give a list of files, use the following two options. The list files" << endl;
        cerr << "should contain one filename on each line." << endl;
        cerr << "  --query-file-list [filename]" << endl;
        cerr << "  --outfile-list [filename]" << endl;
        cerr << "The index must be built before running this program. Specify the location" << endl;
        cerr << "of the index with the following option:" << endl;
        cerr << "  --index-dir [path] (always required, directory must exist before running)" << endl;
        cerr << "The program needs to some disk space to run. Specify a directory for the" << endl;
        cerr << "temporary disk files with the following option:" << endl;
        cerr << "  --temp-dir [path] (always required, directory must exist before running)" << endl;
        cerr << "If you want to align also to the reverse complement, give the following:" << endl;
        cerr << "  --rc (optional, aligns with the reverse complement also)" << endl;
        cerr << "The number of worked threads is given with the following option: " << endl;
        cerr << "  --threads (optional, default 1)" << endl;
        cerr << "Additional memory allowed on top of the index structure:" << endl;
        cerr << "  --mem-megas [number] (optional. Default: 1000)" << endl;
        cerr << endl;
        cerr << "Usage examples:" << endl;
        cerr << "Pseudoalign reads.fna against an index:" << endl;
        cerr << "  ./pseudoalign --query-file reads.fna --index-dir index --temp-dir temp --outfile out.txt" << endl;
        cerr << "Pseudoalign reads.fna against an index using also reverse complements:" << endl;
        cerr << "  ./pseudoalign --rc --query-file reads.fna --index-dir index --temp-dir temp --outfile out.txt" << endl;
        exit(1);
    }

    Config C;
    for(auto keyvalue : parse_args(argc, argv)){
        string option = keyvalue.first;
        vector<string> values = keyvalue.second;
        if(option == "--query-file"){
            assert(values.size() == 1);
            C.query_files.push_back(values[0]);
        } else if(option == "--query-file-list"){
            assert(values.size() == 1);
            C.query_files = read_lines(values[0]);
        } else if(option == "--index-dir"){
            assert(values.size() == 1);
            C.index_dir = values[0];
        } else if(option == "--temp-dir"){
            assert(values.size() == 1);
            C.temp_dir = values[0];
        } else if(option == "--outfile"){
            assert(values.size() == 1);
            C.outfiles.push_back(values[0]);  
        } else if(option == "--outfile-list"){
            assert(values.size() == 1);
            C.outfiles = read_lines(values[0]);
        } else if(option == "--rc"){
            C.reverse_complements = true;
        } else if(option == "--threads"){
            assert(values.size() == 1);
            C.n_threads = stoll(values[0]);
        } else if(option == "--mem-megas"){
            assert(values.size() == 1);
            C.memory_megas = std::stoll(values[0]);
        } else{
            cerr << "Error parsing command line arguments. Unkown option: " << option << endl;
            exit(1);
        }
    }

    C.check_valid();

    write_log("Starting");

    temp_file_manager.set_dir(C.temp_dir);

    write_log("Loading the index");    
    KallistoLite kl;
    kl.load_boss(C.index_dir + "/boss-");
    kl.load_colors(C.index_dir + "/coloring-");

    for(LL i = 0; i < C.query_files.size(); i++){
        write_log("Aligning " + C.query_files[i] + " (writing output to " + C.outfiles[i] + ")");
        // TODO: RESPECT RAM BOUND
        kl.pseudoalign_parallel(C.n_threads, C.query_files[i], C.outfiles[i], C.reverse_complements, 1000000); // Buffer size 1 MB
        temp_file_manager.clean_up();
    }

    write_log("Finished");
}
