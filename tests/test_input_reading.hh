#pragma once

#include <gtest/gtest.h>
#include "../globals.hh"
#include "setup_tests.hh"

void check_sequence_reader_output(const vector<string>& seqs, string fastafile){
    Sequence_Reader sr(fastafile, FASTA_MODE);
    for(string seq : seqs){
        ASSERT_FALSE(sr.done());        
        ASSERT_EQ(sr.get_next_query_stream().get_all(), seq);
    }
    ASSERT_TRUE(sr.done());
}

TEST(INPUT_PARSING, fasta_basic){
    vector<string> seqs = {"AAGTGCTGTANAYA","ACGTURYKMSWBDHVN-"};
    string fasta = ">\n" + seqs[0] + "\n>\n" + seqs[1] + "\n";
    logger << fasta << endl << seqs << endl;
    string filename = string_to_temp_file(fasta);
    check_sequence_reader_output(seqs, filename);

}

TEST(INPUT_PARSING, fasta_multiple_lines){
    vector<string> seqs = {"AAGTGCTGTANAYA","ACGTURYKMSWBDHVN-"};
    string fasta;

    // Write 3 chars per line
    for(string seq : seqs){
        fasta += ">\n";
        for(LL i = 0; i < (LL)seq.size(); i += 3){
            fasta += seq.substr(i,3) + "\n";
        }
    }
    logger << fasta << endl << seqs << endl;
    string filename = string_to_temp_file(fasta);
    check_sequence_reader_output(seqs, filename);
}

TEST(INPUT_PARSING, fasta_upper_case){
    vector<string> seqs = {"AagTGCtGTaNAYA","AcGTURYKmSWbDHVn-"};
    string fasta;

    for(string seq : seqs) fasta += ">\n" + seq + "\n";
    
    logger << fasta << endl << seqs << endl;
    string filename = string_to_temp_file(fasta);

    for(string& seq : seqs) for(char& c : seq) c = toupper(c); // Upper case for validation
    
    check_sequence_reader_output(seqs, filename);
}

TEST(INPUT_PARSING, fasta_super_long_line){
    vector<string> seqs;
    seqs.push_back(string(1e6, 'A'));
    seqs.push_back(string(1e5, 'G'));

    string fasta;
    for(string seq : seqs) fasta += ">\n" + seq + "\n";
    string filename = string_to_temp_file(fasta);
    check_sequence_reader_output(seqs, filename);
}

TEST(INPUT_PARSING, fasta_headers){
    vector<string> seqs;
    seqs.push_back(string(1e6, 'A'));
    seqs.push_back(string(1e5, 'G'));

    vector<string> headers;
    headers.push_back(string(1e5, 'h'));
    headers.push_back(string(1e6, 'H'));

    string fasta;
    for(LL i = 0; i < seqs.size(); i++) fasta += ">" + headers[i] + "\n" + seqs[i] + "\n";
    string filename = string_to_temp_file(fasta);
    Sequence_Reader sr(filename, FASTA_MODE);
    Read_stream rs = sr.get_next_query_stream();
    ASSERT_EQ(rs.header, ">" + headers[0]);
    rs.get_all();
    rs = sr.get_next_query_stream();
    ASSERT_EQ(rs.header, ">" + headers[1]);
    rs.get_all();
}

//TEST(INPUT_PARSING, fastq){
    // Check upper-casing

    // Check multiple lines

    // Check super long line (should not overflow any buffer)

    // @-symbols in quality line

    // +-symbols in quality line


//}