#include "Themisto.hh"
#include "input_reading.hh"
#include "zpipe.hh"
#include <string>
#include <cstring>
#include "version.h"
#include "cxxopts.hpp"
#include "extract_unitigs.hh"

using namespace std;

int stats_main(int argc, char** argv){

    cxxopts::Options options(argv[0], "Extract unitigs out of the Themisto index.");

    options.add_options()
        ("i,index-dir", "Location of the Themisto index.", cxxopts::value<string>())
        ("temp-dir", "Directory for temporary files.", cxxopts::value<string>())
        ("h,help", "Print usage")
    ;

    LL old_argc = argc; // Must store this because the parser modifies it
    auto opts = options.parse(argc, argv);

    if (old_argc == 1 || opts.count("help")){
        std::cerr << options.help() << std::endl;
        return 1;
    }

    get_temp_file_manager().set_dir(opts["temp-dir"].as<string>());
    string index_dir = opts["index-dir"].as<string>();
    check_true(index_dir != "", "Index directory not set");
    check_dir_exists(index_dir);

    Themisto themisto;

    write_log("Loading the index");    
    themisto.load_from_directory(index_dir);

    write_log("Computing index statistics");
    vector<bool> dummy_marks = themisto.boss.get_dummy_node_marks();

    LL total_nodes = themisto.boss.number_of_nodes();
    LL dummy_nodes = 0;
    for(bool b : dummy_marks) if(b) dummy_nodes++;

    LL total_edges = themisto.boss.number_of_edges();
    LL dummy_edges = 0;
    for(LL v = 0; v < themisto.boss.number_of_nodes(); v++){
        if(dummy_marks[v]) dummy_edges += themisto.boss.outdegree(v);
    }

    UnitigExtractor UE;
    string unitigs_file = get_temp_file_manager().create_filename("unitigs-");
    throwing_ofstream unitigs_out(unitigs_file);
    NullStream null_stream;
    write_log("Extracting unitigs");
    UE.extract_unitigs(themisto, unitigs_out.stream, false, null_stream);
    
    LL unitig_count = 0;
    Sequence_Reader sr(unitigs_file, FASTA_MODE);
    LL min_unitig_len = 1e18;
    LL max_unitig_len = 0;
    LL unitig_len_sum = 0;
    while(!sr.done()){
        string unitig = sr.get_next_query_stream().get_all();
        unitig_count++;
        min_unitig_len = min(min_unitig_len, (LL)unitig.size());
        max_unitig_len = max(max_unitig_len, (LL)unitig.size());
        unitig_len_sum += unitig.size();
    }

    cout << "Node length k: " << themisto.boss.get_k() << endl;
    cout << "Node length k+1: " << themisto.boss.get_k() + 1 << endl;
    cout << "Node count: " << total_nodes - dummy_nodes << endl;
    cout << "Node count (including technical BOSS dummy nodes): " << total_nodes << endl;
    cout << "Edge count: " << total_edges - dummy_edges << endl;
    cout << "Edge count (including technical BOSS dummy edges): " << total_edges << endl;
    cout << "Min unitig length: " << min_unitig_len << endl;    
    cout << "Max unitig length: " << max_unitig_len << endl;
    cout << "Avg unitig length: " << (double)unitig_len_sum / unitig_count << endl;

    return 0;
    
}