#include "coloring/Coloring.hh"
#include "globals.hh"
#include "zpipe.hh"
#include <string>
#include <cstring>
#include "version.h"
#include "cxxopts.hpp"

using namespace sbwt;
using namespace std;

int color_set_diagnostics_main(int argc, char** argv){

    cxxopts::Options options(argv[0], "Prints stuff avoid the coloring data structure. For developers.");

    options.add_options()
        ("i", "The index prefix that was given to the build command.", cxxopts::value<string>())
        ("o", "The index prefix for the output.", cxxopts::value<string>())
        ("h,help", "Print usage")
    ;

    int64_t old_argc = argc; // Must store this because the parser modifies it
    auto opts = options.parse(argc, argv);

    if (old_argc == 1 || opts.count("help")){
        std::cerr << options.help() << std::endl;
        return 1;
    }

    string input_dbg_file = opts["i"].as<string>() + ".tdbg";
    string input_color_file = opts["i"].as<string>() + ".tcolors";

    string output_dbg_file = opts["o"].as<string>() + ".tdbg";
    string output_color_file = opts["o"].as<string>() + ".tcolors";

    write_log("Loading the index", LogLevel::MAJOR);
    plain_matrix_sbwt_t SBWT;
    Coloring<> coloring;

    cerr << "Loading SBWT" << endl;
    SBWT.load(input_dbg_file);
    cerr << "Loading coloring" << endl;
    coloring.load(input_color_file, SBWT);

    cerr << "Building backward traversal support" << endl;
    SBWT_backward_traversal_support backward_support(&SBWT);

    coloring.add_all_node_id_to_color_set_id_pointers(SBWT, backward_support);

    SBWT.serialize(output_dbg_file);
    coloring.serialize(output_color_file);

    return 0;
    
}