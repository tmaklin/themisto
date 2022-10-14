#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <algorithm>
#include <cassert>
#include <set>
#include <unordered_map>
#include <map>
#include <gtest/gtest.h>
#include <cassert>
#include "sdsl_color_set.hh"

using namespace sbwt;

vector<uint64_t> get_sparse_example(){
    vector<uint64_t> v = {4, 1534, 4003, 8903};
    return v;
}

vector<uint64_t> get_dense_example(int64_t gap, int64_t total_length){
    vector<uint64_t> v;
    for(int64_t i = 0; i < total_length; i += gap){
        v.push_back(i);
    }
    return v;
}

TEST(TEST_COLOR_SET, sparse){
    vector<uint64_t> v = get_sparse_example();
    Bitmap_Or_Deltas_ColorSet cs(v);

    ASSERT_FALSE(cs.is_bitmap);

    vector<uint64_t> v2 = cs.get_colors_as_vector();

    ASSERT_EQ(v,v2);
}

TEST(TEST_COLOR_SET, dense){
    vector<uint64_t> v = get_dense_example(3, 1000);
    Bitmap_Or_Deltas_ColorSet cs(v);
    ASSERT_TRUE(cs.is_bitmap);

    vector<uint64_t> v2 = cs.get_colors_as_vector();

    ASSERT_EQ(v,v2);
}

TEST(TEST_COLOR_SET, sparse_vs_sparse){

    vector<uint64_t> v1 = {4, 1534, 4003, 8903};
    vector<uint64_t> v2 = {4, 2000, 4003, 5000};

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_FALSE(c1.is_bitmap);
    ASSERT_FALSE(c2.is_bitmap);

    vector<uint64_t> v12 = c1.intersection(c2).get_colors_as_vector();

    vector<uint64_t> correct = {4,4003};
    ASSERT_EQ(v12, correct);
}


TEST(TEST_COLOR_SET, dense_vs_dense){

    vector<uint64_t> v1 = get_dense_example(2, 1000); // Multiples of 2
    vector<uint64_t> v2 = get_dense_example(3, 1000); // Multiples of 3

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_TRUE(c1.is_bitmap);
    ASSERT_TRUE(c2.is_bitmap);

    vector<uint64_t> v12 = c1.intersection(c2).get_colors_as_vector();

    vector<uint64_t> correct = get_dense_example(6, 1000); // 6 = lcm(2,3)
    ASSERT_EQ(v12, correct);
}

TEST(TEST_COLOR_SET, sparse_vs_dense){

    vector<uint64_t> v1 = get_dense_example(3, 10000); // Multiples of 3
    vector<uint64_t> v2 = {3, 4, 5, 3000, 6001, 9999};

    Bitmap_Or_Deltas_ColorSet c1(v1);
    Bitmap_Or_Deltas_ColorSet c2(v2);

    ASSERT_TRUE(c1.is_bitmap);
    ASSERT_FALSE(c2.is_bitmap);

    vector<uint64_t> v12 = c1.intersection(c2).get_colors_as_vector();

    vector<uint64_t> correct = {3, 3000, 9999};
    ASSERT_EQ(v12, correct);
}