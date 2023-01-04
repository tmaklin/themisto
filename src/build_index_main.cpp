//#include "Themisto.hh"
#include "zpipe.hh"
#include <string>
#include <cstring>
#include "version.h"
#include "sbwt/globals.hh"
#include "sbwt/variants.hh"
#include "coloring/Coloring.hh"
#include "globals.hh"
#include "sbwt/SeqIO.hh"
#include "sbwt/cxxopts.hpp"
#include "coloring/Coloring_Builder.hh"

using namespace std;

// A color stream for the metadata stream in WorkDispatcher
// If reverse complements, then duplicates colors like: 0,1,2... -> 0,0,1,1,2,2...
class Colorfile_Stream : public Metadata_Stream{

public:

    std::unique_ptr<Buffered_ifstream<>> in;
    string line; // Buffer for reading lines
    vector<string> filenames;
    int64_t current_file_idx = 0;
    int64_t x; // The current color
    bool reverse_complements;
    bool rc_flag = false; // For duplicating colors

    Colorfile_Stream(const vector<string>& filenames, bool reverse_complements) : filenames(filenames), reverse_complements(reverse_complements) {
        if(filenames.size() == 0){
            throw std::runtime_error("Error: empty color file list");
        }
        in = make_unique<Buffered_ifstream<>>(filenames[0]);
    }

    virtual std::array<uint8_t, 8> next(){

        if(reverse_complements && rc_flag){
            // Don't read next color from file but re-use previous
        } else{
            while(!in->getline(line)){
                current_file_idx++;
                if(current_file_idx >= filenames.size())
                    throw std::runtime_error("Error: more colors than sequences.");
                in = make_unique<Buffered_ifstream<>>(filenames[current_file_idx]);
            }
            x = fast_string_to_int(line.c_str(), line.size()); // Parse
        }

        // Return as std::array<uint8_t, 8>
        std::array<uint8_t, 8> ret;
        int64_t* ptr = (int64_t*)ret.data(); // Interpret as int64_t
        *ptr = x;

        rc_flag = !rc_flag;
        return ret;
    }

};


// A colors stream with a unique color for each file
// If reverse complements, then generates each color twice like: 0,0,1,1,2,2... -> 0,0,0,0,1,1,1,1,2,2,2,2...
class Unique_For_Each_File_Color_Stream : public Metadata_Stream{

public:

    vector<int64_t> seq_count_in_file;
    int64_t cur_file_idx = 0;
    int64_t cur_file_seq_idx = 0;
    int64_t x = 0; // Current color
    bool reverse_complements = false;

    Unique_For_Each_File_Color_Stream(const vector<string>& filenames, bool reverse_complements) : reverse_complements(reverse_complements){
        write_log("Counting sequences in input files", sbwt::LogLevel::MAJOR);
        for(const string& filename : filenames){
            seq_count_in_file.push_back(sbwt::SeqIO::count_sequences(filename));
            if(reverse_complements) seq_count_in_file.back() *= 2;
        }
    }

    virtual std::array<uint8_t, 8> next(){
        
        while(cur_file_seq_idx >= seq_count_in_file[cur_file_idx]){
            cur_file_idx++;
            cur_file_seq_idx = 0;
        }

        // Return as std::array<uint8_t, 8>
        std::array<uint8_t, 8> ret;
        int64_t* ptr = (int64_t*)ret.data(); // Interpret as int64_t
        *ptr = cur_file_idx;

        cur_file_seq_idx++;

        return ret;
    }
};

// A color stream that generates colors 0,1,2... if no reverse complements,
// otherwise 0,0,1,1,2,2,..
class Unique_For_Each_Sequence_Color_Stream : public Metadata_Stream{

public:

    int64_t x; // The current color
    bool reverse_complements;
    bool increment_flag;

    Unique_For_Each_Sequence_Color_Stream(bool reverse_complements) : x(0), reverse_complements(reverse_complements), increment_flag(false){}

    virtual std::array<uint8_t, 8> next(){
        // Return as std::array<uint8_t, 8>
        std::array<uint8_t, 8> ret;
        int64_t* ptr = (int64_t*)ret.data(); // Interpret as int64_t
        *ptr = x;

        if(!reverse_complements || increment_flag) x++;
        increment_flag = !increment_flag;

        return ret;
    }

};

struct Build_Config{
    int64_t k = 0;
    int64_t n_threads = 1;
    string seqfile_CLI_variable;
    string colorfile_CLI_variable;
    vector<string> seqfiles;
    vector<string> colorfiles;
    string index_dbg_file;
    string index_color_file;
    string temp_dir;
    string coloring_structure_type;
    string from_index;
    sbwt::SeqIO::FileFormat input_format;
    bool load_dbg = false;
    int64_t memory_megas = 1000;
    bool no_colors = false;
    bool del_non_ACGT = false;
    int64_t colorset_sampling_distance = 1;
    bool verbose = false;
    bool silent = false;
    bool reverse_complements = false;

    bool manual_colors = false;
    bool file_colors = false;
    bool sequence_colors = false;
    
    void check_valid(){

        if(from_index != ""){
            // From existing index
            sbwt::check_true(!manual_colors, "Must not give both --from-index and manual colors");
            sbwt::check_true(seqfiles.size() == 0, "Must not give both --from-index and input sequences");
            sbwt::check_true(no_colors == false, "Must not give both --from-index and --load-dbg");
            sbwt::check_true(k == 0, "Must not give both --from-index and -k because k is defined in the index");
        } else{
            // From fasta
            sbwt::check_true(seqfiles.size() > 0, "Input file not set");
            for(const string& S : seqfiles)
                sbwt::check_readable(S);
            if(!load_dbg){
                sbwt::check_true(k != 0, "Parameter k not set");
                sbwt::check_true(k <= MAX_KMER_LENGTH, "Maximum allowed k is " + std::to_string(MAX_KMER_LENGTH) + ". To increase the limit, recompile by first running cmake with the option `-DMAX_KMER_LENGTH=n`, where n is a number up to 255, and then running `make` again."); // 255 is max because of KMC
            } else{
                if(k != 0){
                    sbwt::write_log("Warning: value of parameter k is ignored because the DBG is not built, but loaded from disk instead", sbwt::LogLevel::MAJOR);
                }
            }
        }

        sbwt::check_writable(index_dbg_file);
        sbwt::check_writable(index_color_file);

        if(colorfiles.size() > 0){
            sbwt::check_true(!no_colors, "Must not give both --no-colors and --manual-colors");
            sbwt::check_true(!file_colors, "Must not give both --no-colors and --file-colors");
            sbwt::check_true(!sequence_colors, "Must not give both --no-colors and --sequence-colors");
            for(const string& S : colorfiles)
                sbwt::check_readable(S);
        }

        if(coloring_structure_type != "sdsl-hybrid" && coloring_structure_type != "roaring"){
            throw std::runtime_error("Unknown coloring structure type: " + coloring_structure_type);
        }

        sbwt::check_true(temp_dir != "", "Temp directory not set");
        check_dir_exists(temp_dir);

        sbwt::check_true(memory_megas > 0, "Memory budget must be positive");
        sbwt::check_true(colorset_sampling_distance >= 1, "Colorset sampling distance must be positive");

    }

    string to_string(){
        stringstream ss;
        if(seqfile_CLI_variable != ""){
            ss << "Sequence file = " << seqfile_CLI_variable << "\n";
            //ss << "Input format = " << input_format.extension << "\n";
        } else{
            ss << "Building from index prefix = " << from_index << "\n";
        }
        if(colorfile_CLI_variable != "") ss << "Color name file = " << colorfile_CLI_variable << "\n";
        ss << "Index de Bruijn graph output file = " << index_dbg_file << "\n";
        ss << "Index coloring output file = " << index_color_file << "\n";
        ss << "Temporary directory = " << temp_dir << "\n";
        ss << "k = " << k << "\n";
        ss << "Reverse complements = " << (reverse_complements ? "true" : "false") << "\n";
        ss << "Number of threads = " << n_threads << "\n";
        ss << "Memory megabytes = " << memory_megas << "\n";
        ss << "Manual colors = " << (manual_colors ? "true" : "false") << "\n";
        ss << "Sequence colors = " << (sequence_colors ? "true" : "false") << "\n";
        ss << "File colors = " << (file_colors ? "true" : "false") << "\n";
        ss << "Load DBG = " << (load_dbg ? "true" : "false") << "\n";
        ss << "Handling of non-ACGT characters = " << (del_non_ACGT ? "delete" : "randomize") << "\n";
        ss << "Coloring structure type: " << coloring_structure_type << "\n"; 

        string verbose_level = "normal";
        if(verbose) verbose_level = "verbose";
        if(silent) verbose_level = "silent";

        ss << "Verbosity = " << verbose_level; // Last has no endline

        return ss.str();
    }
};

// Returns filename of a new color file that has one color for each sequence
template<typename sequence_reader_t>
string generate_default_colorfile(sequence_reader_t& reader, bool reverse_complements){
    string colorfile = sbwt::get_temp_file_manager().create_filename();
    sbwt::Buffered_ofstream<> out(colorfile);
    stringstream ss;
    int64_t seq_id = 0;
    while(true){
        int64_t len = reader.get_next_read_to_buffer();
        if(len == 0) break;
        ss.str(""); ss << seq_id << "\n";
        if(reverse_complements) ss << seq_id << "\n"; // Same color for the reverse complement pair
        out.write(ss.str().data(), ss.str().size());
        seq_id++;
    }
    return colorfile;
}


// Builds and serializes to disk
template<typename colorset_t>
void build_coloring(plain_matrix_sbwt_t& dbg, Metadata_Stream* cfs, const Build_Config& C){

    Coloring<colorset_t> coloring;
    if(C.input_format.gzipped){
        typedef sbwt::SeqIO::Multi_File_Reader<sbwt::SeqIO::Reader<Buffered_ifstream<zstr::ifstream>>> reader_t; // gzipped
        Coloring_Builder<colorset_t, reader_t> cb;
        reader_t reader(C.seqfiles);
        if(C.reverse_complements) reader.enable_reverse_complements();
        cb.build_coloring(coloring, dbg, reader, cfs, C.memory_megas * (1 << 20), C.n_threads, C.colorset_sampling_distance);
    } else{
        typedef sbwt::SeqIO::Multi_File_Reader<sbwt::SeqIO::Reader<Buffered_ifstream<std::ifstream>>> reader_t; // not gzipped
        Coloring_Builder<colorset_t, reader_t> cb; // Builder without gzipped input
        reader_t reader(C.seqfiles);
        if(C.reverse_complements) reader.enable_reverse_complements();
        cb.build_coloring(coloring, dbg, reader, cfs, C.memory_megas * (1 << 20), C.n_threads, C.colorset_sampling_distance);        
    }
    sbwt::throwing_ofstream out(C.index_color_file, ios::binary);
    coloring.serialize(out.stream);
}

// Builds from existing index and serializes to disk
template<typename old_coloring_t, typename new_coloring_t> 
void build_from_index(plain_matrix_sbwt_t& dbg, const old_coloring_t& old_coloring, const Build_Config& C){

    // TODO: This makes a ton of unnecessary copies of things and has high peak RAM

    write_log("Building new structure of type " + C.coloring_structure_type, LogLevel::MAJOR);

    vector<typename new_coloring_t::colorset_type> new_colorsets;

    int64_t largest_color = 0;
    int64_t total_length = 0;

    for(int64_t i = 0; i < old_coloring.number_of_distinct_color_sets(); i++){
        vector<int64_t> set = old_coloring.get_color_set_as_vector_by_color_set_id(i);

        for(int64_t x : set) largest_color = max(largest_color, x);
        total_length += set.size();

        new_colorsets.push_back(set); // Re-encodes as new_coloring_t::color_set_t
    }

    typename new_coloring_t::colorset_storage_type new_storage(new_colorsets);
    new_coloring_t new_coloring(new_storage, 
                                old_coloring.get_node_id_to_colorset_id_structure(),
                                dbg, largest_color, total_length);

    write_log("Serializing to " + C.index_dbg_file + " and " + C.index_color_file, LogLevel::MAJOR);

    throwing_ofstream colors_out(C.index_color_file);
    new_coloring.serialize(colors_out.stream);
    
    throwing_ofstream dbg_out(C.index_dbg_file);
    dbg.serialize(dbg_out.stream);
}

bool has_suffix_dot_txt(const string& S){
    return S.size() >= 4 && S.substr(S.size()-4) == ".txt";
}

int build_index_main(int argc, char** argv_given){

    // Legacy support: transform old option names to new ones
    char** argv = (char**)malloc(sizeof(char*) * argc); // Freed and the and of the function
    char legacy_support_fix[] = "-k";
    char legacy_support_fix2[] = "--manual-colors";
    char legacy_support_fix3[] = "--sequence-colors";
    argv[0] = argv_given[0];
    for(int64_t i = 1; i < argc; i++){
        if(string(argv_given[i]) == "--k") argv[i] = legacy_support_fix;
        else if(string(argv_given[i]) == "--color-file") argv[i] = legacy_support_fix2;
        else if(string(argv_given[i]) == "--auto-colors") argv[i] = legacy_support_fix3;
        else argv[i] = argv_given[i];
    }

    cxxopts::Options options(argv[0], "Builds an index consisting of compact de Bruijn graph using the Wheeler graph data structure and color information. The input is a set of reference sequences in a single file in fasta or fastq format, and a colorfile, which is a plain text file containing the colors (integers) of the reference sequences in the same order as they appear in the reference sequence file, one line per sequence. If there are characters outside of the DNA alphabet ACGT in the input sequences, those are replaced with random characters from the DNA alphabet.");

    options.add_options()
        ("k,node-length", "The k of the k-mers.", cxxopts::value<int64_t>()->default_value("0"))
        ("i,input-file", "The input sequences in FASTA or FASTQ format. The format is inferred from the file extension. Recognized file extensions for fasta are: .fasta, .fna, .ffn, .faa and .frn . Recognized extensions for fastq are: .fastq and .fq.", cxxopts::value<string>()->default_value(""))
        ("c,manual-colors", "A file containing one integer color per sequence, one color per line. If there are multiple sequence files, then this file should be a text file containing the corresponding color filename for each sequence file, one filename per line. ", cxxopts::value<string>()->default_value(""))
        ("f,file-colors", "Creates a distinct color 0,1,2,... for each file in the input file list, in the order the files appear in the list", cxxopts::value<bool>()->default_value("false"))
        ("e,sequence-colors", "Creates a distinct color 0,1,2,... for each sequence in the input, in the order the sequences are processed. This is the default behavior if no other color options are given.", cxxopts::value<bool>()->default_value("false"))
        ("no-colors", "Build only the de Bruijn graph without colors.", cxxopts::value<bool>()->default_value("false"))
        ("o,index-prefix", "The de Bruijn graph will be written to [prefix].tdbg and the color structure to [prefix].tcolors.", cxxopts::value<string>())
        ("r,reverse-complements", "Also add reverse complements of the k-mers to the index.", cxxopts::value<bool>()->default_value("false"))
        ("temp-dir", "Directory for temporary files. This directory should have fast I/O operations and should have as much space as possible.", cxxopts::value<string>())
        ("m,mem-megas", "Number of megabytes allowed for external memory algorithms (must be at least 2048).", cxxopts::value<int64_t>()->default_value("2048"))
        ("t,n-threads", "Number of parallel exectuion threads. Default: 1", cxxopts::value<int64_t>()->default_value("1"))
        ("randomize-non-ACGT", "Replace non-ACGT letters with random nucleotides. If this option is not given, k-mers containing a non-ACGT character are deleted instead.", cxxopts::value<bool>()->default_value("false"))
        ("d,colorset-pointer-tradeoff", "This option controls a time-space tradeoff for storing and querying color sets. If given a value d, we store color set pointers only for every d nodes on every unitig. The higher the value of d, the smaller then index, but the slower the queries. The savings might be significant if the number of distinct color sets is small and the graph is large and has long unitigs.", cxxopts::value<int64_t>()->default_value("1"))
        ("load-dbg", "If given, loads a precomputed de Bruijn graph from the index prefix. If this is given, the value of parameter -k is ignored because the order k is defined by the precomputed de Bruijn graph.", cxxopts::value<bool>()->default_value("false"))
        ("s,coloring-structure-type", "Type of coloring structure to build (\"sdsl-hybrid\", \"roaring\").", cxxopts::value<string>()->default_value("sdsl-hybrid"))
        ("from-index", "Take as input a pre-built Themisto index. Builds a new index in the format specified by --coloring-structure-type. This is currenlty implemented by decompressing the distinct color sets in memory before re-encoding them, so this might take a lot of RAM.", cxxopts::value<string>()->default_value(""))
        ("v,verbose", "More verbose progress reporting into stderr.", cxxopts::value<bool>()->default_value("false"))
        ("silent", "Print as little as possible to stderr (only errors).", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage")
    ;

    int64_t old_argc = argc; // Must store this because the parser modifies it
    auto opts = options.parse(argc, argv);

    if (old_argc == 1 || opts.count("help")){
        std::cerr << options.help() << std::endl;
        cerr << "Usage examples:" << endl;
        cerr << "Build the de Bruijn graph and colors:" << endl;
        cerr << "  " << argv[0] << " -k 31 --mem-megas 10000 --input-file references.fna --color-file colors.txt --index-prefix my_index --temp-dir temp" << endl;
        cerr << "Build only the de Bruijn graph" << endl;
        cerr << "  " << argv[0] << " -k 31 --mem-megas 10000 --input-file references.fna --index-prefix my_index --temp-dir temp --no-colors" << endl;
        cerr << "Load a previously built de Bruijn graph from the index directory and compute the colors:" << endl;
        cerr << "  " << argv[0] << " --mem-megas 10000 --input-file references.fna --color-file colors.txt --index-prefix my_index --temp-dir temp --load-dbg" << endl;
        exit(1);
    }    

    Build_Config C;
    C.k = opts["k"].as<int64_t>();
    C.n_threads = opts["n-threads"].as<int64_t>();
    C.index_dbg_file = opts["index-prefix"].as<string>() + ".tdbg";
    C.index_color_file = opts["index-prefix"].as<string>() + ".tcolors";
    C.temp_dir = opts["temp-dir"].as<string>();
    C.load_dbg = opts["load-dbg"].as<bool>();
    C.memory_megas = opts["mem-megas"].as<int64_t>();
    C.no_colors = opts["no-colors"].as<bool>();
    C.colorset_sampling_distance = opts["colorset-pointer-tradeoff"].as<int64_t>();
    C.del_non_ACGT = !(opts["randomize-non-ACGT"].as<bool>());
    C.verbose = opts["verbose"].as<bool>();
    C.silent = opts["silent"].as<bool>();
    C.coloring_structure_type = opts["coloring-structure-type"].as<string>();
    C.reverse_complements = opts["reverse-complements"].as<bool>();
    C.file_colors = opts["file-colors"].as<bool>();
    C.sequence_colors = opts["sequence-colors"].as<bool>();
    C.from_index = opts["from-index"].as<string>();

    C.colorfile_CLI_variable = opts["manual-colors"].as<string>();
    C.seqfile_CLI_variable = opts["input-file"].as<string>();

    if(C.colorfile_CLI_variable == "" && !C.file_colors && !C.sequence_colors)
        C.sequence_colors = true; // Default behavior if no color options are set

    C.manual_colors = !C.file_colors && !C.sequence_colors;

    if(has_suffix_dot_txt(C.seqfile_CLI_variable)){
        // List of filenames
        C.seqfiles = sbwt::readlines(C.seqfile_CLI_variable);
        if(C.manual_colors)
            C.colorfiles = sbwt::readlines(C.colorfile_CLI_variable);
    } else{
        // Single file
        if(C.seqfile_CLI_variable != "")
            C.seqfiles = {C.seqfile_CLI_variable};
        if(C.manual_colors)
            C.colorfiles = {C.colorfile_CLI_variable};
    }

    if(C.seqfiles.size() > 0)
        C.input_format = sbwt::SeqIO::figure_out_file_format(C.seqfiles[0]);

    if(C.verbose && C.silent) throw runtime_error("Can not give both --verbose and --silent");
    if(C.verbose) set_log_level(sbwt::LogLevel::MINOR);
    if(C.silent) set_log_level(sbwt::LogLevel::OFF);

    create_directory_if_does_not_exist(C.temp_dir);

    C.check_valid();
    sbwt::get_temp_file_manager().set_dir(C.temp_dir);

    write_log("Build configuration:\n" + C.to_string(), sbwt::LogLevel::MAJOR);
    write_log("Starting", sbwt::LogLevel::MAJOR);

    if(C.from_index != ""){
        // Transform existing index to new format

        sbwt::write_log("Loading de Bruijn Graph", sbwt::LogLevel::MAJOR);
        std::unique_ptr<sbwt::plain_matrix_sbwt_t> dbg_ptr = std::make_unique<sbwt::plain_matrix_sbwt_t>();
        dbg_ptr->load(C.from_index + ".tdbg");

        sbwt::write_log("Loading coloring", sbwt::LogLevel::MAJOR);
        std::variant<Coloring<SDSL_Variant_Color_Set>, Coloring<Roaring_Color_Set>> old_coloring;
        load_coloring(C.from_index + ".tcolors", *dbg_ptr, old_coloring);

        if(std::holds_alternative<Coloring<SDSL_Variant_Color_Set>>(old_coloring))
            write_log("sdsl coloring structure loaded", LogLevel::MAJOR);
        if(std::holds_alternative<Coloring<Roaring_Color_Set>>(old_coloring))
            write_log("roaring coloring structure loaded", LogLevel::MAJOR);

        auto visitor = [&](auto& old){
            if(C.coloring_structure_type == "sdsl-hybrid"){
                build_from_index<decltype(old), Coloring<SDSL_Variant_Color_Set>>(*dbg_ptr, old, C);
            } else if(C.coloring_structure_type == "roaring"){
                build_from_index<decltype(old), Coloring<Roaring_Color_Set>>(*dbg_ptr, old, C);
            } else{
                throw std::runtime_error("Unkown coloring structure type: " + C.coloring_structure_type);
            }
        };

        std::visit(visitor, old_coloring);

        return 0;

    }

    // Deal with non-ACGT characters
    if(C.del_non_ACGT){
        // KMC takes care of this
    } else {
        write_log("Replacing non-ACGT characters with random nucleotides", LogLevel::MAJOR);
        for(string& S : C.seqfiles){
            S = fix_alphabet(S); // Turns the file into fasta format also
            C.input_format = sbwt::SeqIO::figure_out_file_format(S);
        }
    }

    std::unique_ptr<Metadata_Stream> color_stream;

    if(!C.no_colors){
        if(C.file_colors){
            color_stream = make_unique<Unique_For_Each_File_Color_Stream>(C.seqfiles, C.reverse_complements);
        } else if(C.colorfiles.size() == 0){
            // Color each sequence separately
            color_stream = make_unique<Unique_For_Each_Sequence_Color_Stream>(C.reverse_complements);
        } else{
            // User-defined colors
            color_stream = make_unique<Colorfile_Stream>(C.colorfiles, C.reverse_complements);
        }
    }

    std::unique_ptr<sbwt::plain_matrix_sbwt_t> dbg_ptr;

    if(C.load_dbg){
        sbwt::write_log("Loading de Bruijn Graph", sbwt::LogLevel::MAJOR);
        dbg_ptr = std::make_unique<sbwt::plain_matrix_sbwt_t>();
        dbg_ptr->load(C.index_dbg_file);
    } else{
        sbwt::write_log("Building de Bruijn Graph", sbwt::LogLevel::MAJOR);

        vector<string> KMC_input_files = C.seqfiles;
        if(C.reverse_complements){
            write_log("Creating reverse complemented copies of sequence files to " + sbwt::get_temp_file_manager().get_dir(), LogLevel::MAJOR);
            vector<string> rc_files;
            if(C.input_format.gzipped){
                rc_files = sbwt::SeqIO::create_reverse_complement_files<
                    sbwt::SeqIO::Reader<sbwt::Buffered_ifstream<sbwt::zstr::ifstream>>,
                    sbwt::SeqIO::Writer<sbwt::Buffered_ofstream<sbwt::zstr::ofstream>>>(C.seqfiles);
            } else{
                rc_files = (sbwt::SeqIO::create_reverse_complement_files<
                    sbwt::SeqIO::Reader<sbwt::Buffered_ifstream<std::ifstream>>,
                    sbwt::SeqIO::Writer<sbwt::Buffered_ofstream<std::ofstream>>>(C.seqfiles));
            }
            for(string S : rc_files) KMC_input_files.push_back(S);
        }
        
        sbwt::plain_matrix_sbwt_t::BuildConfig sbwt_config;
        sbwt_config.build_streaming_support = true;
        sbwt_config.input_files = KMC_input_files;
        sbwt_config.k = C.k;
        sbwt_config.max_abundance = 1e9;
        sbwt_config.min_abundance = 1;
        sbwt_config.n_threads = C.n_threads;
        sbwt_config.ram_gigas = max((int64_t)2, C.memory_megas / (1 << 10)); // KMC requires at least 2 GB
        sbwt_config.temp_dir = C.temp_dir;
        dbg_ptr = std::make_unique<sbwt::plain_matrix_sbwt_t>(sbwt_config);
        dbg_ptr->serialize(C.index_dbg_file);
        sbwt::write_log("Building de Bruijn Graph finished (" + std::to_string(dbg_ptr->number_of_kmers()) + " k-mers)", sbwt::LogLevel::MAJOR);
    }

    if(!C.no_colors){
        sbwt::write_log("Building colors", sbwt::LogLevel::MAJOR);

        if(C.coloring_structure_type == "sdsl-hybrid"){
            build_coloring<SDSL_Variant_Color_Set>(*dbg_ptr, color_stream.get(), C);
        } else if(C.coloring_structure_type == "roaring"){
            build_coloring<Roaring_Color_Set>(*dbg_ptr, color_stream.get(), C);
        }
    } else{
        std::filesystem::remove(C.index_color_file); // There is an empty file so let's remove it
    }

    sbwt::write_log("Finished", sbwt::LogLevel::MAJOR);

    free(argv);

    return 0;
}

int build_index_main_ggcat(int argc, char** argv){
    cxxopts::Options options(argv[0], "Build the Themisto index using GGCAT. Only file-colors are supported. Reverse complements are always added to the index. Only takes in lists of filenames. Only supports the sdsl-hybrid coloring.");

    options.add_options()
        ("k,node-length", "The k of the k-mers.", cxxopts::value<int64_t>()->default_value("0"))
        ("i,input-file", "The input sequences in FASTA or FASTQ format. The format is inferred from the file extension. Recognized file extensions for fasta are: .fasta, .fna, .ffn, .faa and .frn . Recognized extensions for fastq are: .fastq and .fq.", cxxopts::value<string>()->default_value(""))
        ("o,index-prefix", "The de Bruijn graph will be written to [prefix].tdbg and the color structure to [prefix].tcolors.", cxxopts::value<string>())
        ("temp-dir", "Directory for temporary files. This directory should have fast I/O operations and should have as much space as possible.", cxxopts::value<string>())
        ("m,mem-megas", "Number of megabytes allowed for external memory algorithms (must be at least 2048).", cxxopts::value<int64_t>()->default_value("2048"))
        ("t,n-threads", "Number of parallel exectuion threads. Default: 1", cxxopts::value<int64_t>()->default_value("1"))
        ("d,colorset-pointer-tradeoff", "This option controls a time-space tradeoff for storing and querying color sets. If given a value d, we store color set pointers only for every d nodes on every unitig. The higher the value of d, the smaller then index, but the slower the queries. The savings might be significant if the number of distinct color sets is small and the graph is large and has long unitigs.", cxxopts::value<int64_t>()->default_value("1"))
        ("v,verbose", "More verbose progress reporting into stderr.", cxxopts::value<bool>()->default_value("false"))
        ("silent", "Print as little as possible to stderr (only errors).", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage")
    ;

    int64_t old_argc = argc; // Must store this because the parser modifies it
    auto opts = options.parse(argc, argv);

    if (old_argc == 1 || opts.count("help")){
        std::cerr << options.help() << std::endl;
        exit(1);
    }    

    int64_t k = opts["k"].as<int64_t>();
    int64_t n_threads = opts["n-threads"].as<int64_t>();
    string index_dbg_file = opts["index-prefix"].as<string>() + ".tdbg";
    string index_color_file = opts["index-prefix"].as<string>() + ".tcolors";
    string temp_dir = opts["temp-dir"].as<string>();
    int64_t memory_megas = opts["mem-megas"].as<int64_t>();
    int64_t colorset_sampling_distance = opts["colorset-pointer-tradeoff"].as<int64_t>();
    bool verbose = opts["verbose"].as<bool>(); // TODO: does nothing
    bool silent = opts["silent"].as<bool>(); // TODO: does nothing

    create_directory_if_does_not_exist(temp_dir);
    sbwt::get_temp_file_manager().set_dir(temp_dir);

    // List of filenames
    string seqfile_CLI_variable = opts["input-file"].as<string>();
    vector<string> seqfiles = sbwt::readlines(seqfile_CLI_variable);

    // Run GGCAT
    sbwt::write_log("Running GGCAT", sbwt::LogLevel::MAJOR);
    GGCAT_unitig_database db(seqfiles, max(1LL, memory_megas / (1LL << 10)), k, n_threads, true); // Canonical unitigs

    string unitigfile = db.get_unitig_filename();
    string rev_unitigfile = sbwt::SeqIO::create_reverse_complement_file<
            sbwt::SeqIO::Reader<sbwt::Buffered_ifstream<std::ifstream>>,
            sbwt::SeqIO::Writer<sbwt::Buffered_ofstream<std::ofstream>>>(unitigfile);

    // Build SBWT

    sbwt::write_log("Building SBWT", sbwt::LogLevel::MAJOR);
    sbwt::plain_matrix_sbwt_t::BuildConfig sbwt_config;
    sbwt_config.build_streaming_support = true;
    sbwt_config.input_files = {unitigfile, rev_unitigfile};
    sbwt_config.k = k;
    sbwt_config.max_abundance = 1e9;
    sbwt_config.min_abundance = 1;
    sbwt_config.n_threads = n_threads;
    sbwt_config.ram_gigas = max((int64_t)2, memory_megas / (1 << 10)); // KMC requires at least 2 GB
    sbwt_config.temp_dir = temp_dir;
    sbwt::plain_matrix_sbwt_t SBWT(sbwt_config);
    SBWT.serialize(index_dbg_file);
    sbwt::write_log("Building de Bruijn Graph finished (" + std::to_string(SBWT.number_of_kmers()) + " k-mers)", sbwt::LogLevel::MAJOR);
    
    sbwt::write_log("Building color structure", sbwt::LogLevel::MAJOR);
    Coloring<SDSL_Variant_Color_Set> coloring;
    Coloring_Builder<SDSL_Variant_Color_Set> cb;
    sbwt::SeqIO::Reader reader(unitigfile); reader.enable_reverse_complements();
    cb.build_from_colored_unitigs(coloring, reader, SBWT, max((int64_t)1, memory_megas * (1 << 20)), n_threads, colorset_sampling_distance, db);

    sbwt::write_log("Serializing color structure", sbwt::LogLevel::MAJOR);
    sbwt::throwing_ofstream out(index_color_file, ios::binary);
    coloring.serialize(out.stream);


    sbwt::write_log("Done", sbwt::LogLevel::MAJOR);
    return 0;
}
