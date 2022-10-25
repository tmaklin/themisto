#pragma once

#include <vector>
#include "sdsl/bit_vectors.hpp"
#include "sdsl/int_vector.hpp"
#include "Color_Set_Interface.hh"
#include "SeqIO.hh"
#include <variant>

/*

The purpose of this file is to have a coloring structure that avoids space
overheads associated with allocating each color set from the heap separately.
Instead, here we concatenate all the color sets and store pointers to the starts
of the color sets. 

Actually, there are two concatenations: a concatenation of bit maps and a concatenation 
of integer arrays. A color is stored either as a bit map or an integer array, depending
which one is smaller.

To avoid copying stuff around in memory, we have a color set view class that stores
just the pointer to one of the concatenations. The pointer is stored as a std::variant,
which stores either pointer and contains the bit specifying which type of pointer it is.

But here is the problem. The user might want a mutable color set, for example when doing
intersections of the sets. The static concatenation of sets does not support creating
new sets dynamically. This is why also we have a color set class that allocates and frees
its own memory. This has exactly the same interface as the view class, except it has
functions for set intersection and union, which modify the color set.

Now, to avoid duplicating code between the member functions of the color set view and the
concrete color set, we write the member functions as template functions that take the color
set (view or concrete) as the first parameter. The actual member functions in the color set
and view classes are just proxies that call these template functions. This saves 50+ lines
of duplicated code for member functions that are identical, such as get_colors_as_vector. 

You may be thinking, why not just use inheritance? That is not an option because the view
class needs a const-pointer whereas the other class needs a regular pointer. The view needs
a const-pointer so that the get-colorset method of the coloring storage can be const. The mutable
class needs a regular pointer because, well, it is supposed to be mutable. A std::variant of
a const-pointer and a regular pointer would be possible, but very ugly. Virtual inheritance
is not an option for performance reasons.

*/

using namespace std;

template<typename colorset_t> 
static inline bool colorset_is_empty(const colorset_t& cs){
    return cs.length == 0;
}

template<typename colorset_t> 
static inline bool colorset_is_bitmap(const colorset_t& cs){
    // Check variant type by index because it could be const or non-const and we don't want to care
    return cs.data_ptr.index() == 0;
}

template<typename colorset_t> 
static inline bool colorset_access_bitmap(const colorset_t& cs, int64_t idx){
    // Using std::holds_alternative by index because it could have a const or a non-const type
    return (*std::get<0>(cs.data_ptr))[cs.start + idx];
}

template<typename colorset_t> 
static inline int64_t colorset_access_array(const colorset_t& cs, int64_t idx){
    // Using std::holds_alternative by index because it could have a const or a non-const type
    return (*std::get<1>(cs.data_ptr))[cs.start + idx];
}

template<typename colorset_t> 
static inline int64_t colorset_size(const colorset_t& cs){
    if(colorset_is_bitmap(cs)){
        // Count number of bits set
        int64_t count = 0; 
        for(int64_t i = 0; i < cs.length; i++){
            count += colorset_access_bitmap(cs, i);
        }
        return count;
    } else return cs.length; // Array
}

template<typename colorset_t> 
static inline int64_t colorset_size_in_bits(const colorset_t& cs){
    if(colorset_is_bitmap(cs)) return cs.length;
    else return cs.length * std::get<1>(cs.data_ptr)->width();
    // Using std::holds_alternative by index because it could have a const or a non-const type
}

template<typename colorset_t> 
static inline vector<int64_t> colorset_get_colors_as_vector(const colorset_t& cs){
    std::vector<int64_t> vec;
    if(colorset_is_bitmap(cs)){    
        for(int64_t i = 0; i < cs.length; i++){
            if(colorset_access_bitmap(cs,i)) vec.push_back(i);
        }
    } else{
        for(int64_t i = 0; i < cs.length; i++){
            vec.push_back(colorset_access_array(cs,i));
        }
    }
    return vec;
}

template<typename colorset_t> 
static inline bool colorset_contains(const colorset_t& cs, int64_t color){
    if(colorset_is_bitmap(cs)){
        if(color >= cs.length) return false;
        return colorset_access_bitmap(cs, color);
    } else{
        // Linear scan. VERY SLOW
        for(int64_t i = 0; i < cs.length; i++){
            if(colorset_access_array(cs,i) == color) return true;
        }
        return false;
    }
}

// Stores the intersection into buf1 and returns the number of elements in the
// intersection (does not resize buf1). Buffer elements must be sorted.
// Assumes all elements in a buffer are distinct
int64_t intersect_buffers(sdsl::int_vector<>& buf1, int64_t buf1_len, const sdsl::int_vector<>& buf2, int64_t buf2_len);

// Stores the union into result_buf and returns the number of elements in the
// union (does not resize result_buf). Buffers elements must be sorted.
// Assumes all elements in a buffer are distinct. result_buf must have enough
// space to accommodate the union
int64_t union_buffers(vector<int64_t>& buf1, int64_t buf1_len, vector<int64_t>& buf2, int64_t buf2_len, vector<int64_t>& result_buf);

// Stores the result into A and returns the length of the new bit vector. A is not resized
// but the old elements past the end are left in place to avoid memory reallocations.
int64_t bitmap_vs_bitmap_intersection(sdsl::bit_vector& A, int64_t A_size, const sdsl::bit_vector& B, int64_t B_start, int64_t B_size);

// Stores the result into iv and returns the size of the intersection. iv is not resized
// but the old elements past the end  are left in place to avoid memory reallocations.
int64_t array_vs_bitmap_intersection(sdsl::int_vector<>& iv, int64_t iv_size, const sdsl::bit_vector& bv, int64_t bv_start, int64_t bv_size);

// Stores the result into bv and returns the length of bv. bv is not resized
// but the old elements past the end are left in place to avoid memory reallocations.
int64_t bitmap_vs_array_intersection(sdsl::bit_vector& bv, int64_t bv_size, const sdsl::int_vector<>& iv, int64_t iv_start, int64_t iv_size);

// Stores the result into A and returns the length of the new vector. A is not resized
// but the old elements past the end are left in place to avoid memory reallocations.
int64_t array_vs_array_intersection(sdsl::int_vector<>& A, int64_t A_len, const sdsl::int_vector<>& B, int64_t B_start, int64_t B_len);

// Stores the result into A and returns the length of the new bit vector. A must have enough
// space to accommodate the union
int64_t bitmap_vs_bitmap_union(sdsl::bit_vector& A, int64_t A_size, const sdsl::bit_vector& B, int64_t B_start, int64_t B_size);

// Stores the result into A and returns the length of the new bit vector. A must have enough
// space to accommodate the union
int64_t array_vs_bitmap_union(sdsl::int_vector<>& iv, int64_t iv_size, const sdsl::bit_vector& bv, int64_t bv_start, int64_t bv_size);

// Stores the result into A and returns the length of the new bit vector. A must have enough
// space to accommodate the union
int64_t bitmap_vs_array_union(sdsl::bit_vector& bv, int64_t bv_size, const sdsl::int_vector<>& iv, int64_t iv_start, int64_t iv_size);

// Stores the result into A and returns the length of the new bit vector. A must have enough
// space to accommodate the union
int64_t array_vs_array_union(sdsl::int_vector<>& A, int64_t A_len, const sdsl::int_vector<>& B, int64_t B_start, int64_t B_len);

class Color_Set;

class Color_Set_View{

public:

    std::variant<const sdsl::bit_vector*, const sdsl::int_vector<>*> data_ptr; // Non-owning pointer to external data
    int64_t start;
    int64_t length; // Number of bits in case of bit vector, number of elements in case of array

    Color_Set_View(std::variant<const sdsl::bit_vector*, const sdsl::int_vector<>*> data_ptr, int64_t start, int64_t length)
        : data_ptr(data_ptr), start(start), length(length){}

    Color_Set_View(const Color_Set& cs); // Defined in the .cpp file because Color_Set is not yet defined at this point of this header

    bool empty() const {return colorset_is_empty(*this);};
    bool is_bitmap() const {return colorset_is_bitmap(*this);};
    int64_t size() const {return colorset_size(*this);}
    int64_t size_in_bits() const {return colorset_size_in_bits(*this);}
    bool contains(int64_t color) const {return colorset_contains(*this, color);}
    vector<int64_t> get_colors_as_vector() const {return colorset_get_colors_as_vector(*this);}

};

class Color_Set{

    public:

    typedef Color_Set_View view_t;

    std::variant<sdsl::bit_vector*, sdsl::int_vector<>*> data_ptr = (sdsl::bit_vector*) nullptr; // Owning pointer
    int64_t start = 0;
    int64_t length = 0; // Number of bits in case of bit vector, number of elements in case of array

    Color_Set(){
        data_ptr = new sdsl::bit_vector();
    }

    const Color_Set& operator=(const Color_Set& other){
        if(this == &other) return *this; // Assignment to itself

        // Free existing memory
        auto call_delete = [](auto ptr){delete ptr;};
        std::visit(call_delete, this->data_ptr);

        // Alocate new memory
        if(std::holds_alternative<sdsl::bit_vector*>(other.data_ptr))
            this->data_ptr = new sdsl::bit_vector(*(std::get<sdsl::bit_vector*>(other.data_ptr)));
        else
            this->data_ptr = new sdsl::int_vector<>(*(std::get<sdsl::int_vector<>*>(other.data_ptr)));
        this->start = other.start;
        this->length = other.length;
        return *this;
    }

    Color_Set(const Color_Set& other){
        *this = other;
    }

    // Construct a copy of a color set from a view
    Color_Set(const Color_Set_View& view) : length(view.length){
        if(std::holds_alternative<const sdsl::bit_vector*>(view.data_ptr)){
            data_ptr = new sdsl::bit_vector(view.length, 0);

            // Copy the bits
            const sdsl::bit_vector* from = std::get<const sdsl::bit_vector*>(view.data_ptr);
            sdsl::bit_vector* to = std::get<sdsl::bit_vector*>(data_ptr);
            for(int64_t i = 0; i < view.length; i++){
                (*to)[i] = (*from)[view.start + i];
            }
        } else{
            // Array
            int64_t bit_width = std::get<const sdsl::int_vector<>*>(view.data_ptr)->width();
            data_ptr = new sdsl::int_vector<>(view.length, 0, bit_width);

            // Copy the values
            const sdsl::int_vector<>* from = std::get<const sdsl::int_vector<>*>(view.data_ptr);
            sdsl::int_vector<>* to = std::get<sdsl::int_vector<>*>(data_ptr);
            for(int64_t i = 0; i < view.length; i++){
                (*to)[i] = (*from)[view.start + i];
            }
        }
    }

    Color_Set(const vector<int64_t>& set){
        int64_t max_element = *std::max_element(set.begin(), set.end());
        if(log2(max_element) * set.size() > max_element){ // TODO: this if-statement is duplicated in this file
            // Dense -> bitmap
            sdsl::bit_vector* ptr = new sdsl::bit_vector(max_element+1, 0);
            for(int64_t x : set) (*ptr)[x] = 1;
            length = ptr->size();
            data_ptr = ptr; // Assign to variant
        } else{
            // Sparse -> array
            sdsl::int_vector<>* ptr = new sdsl::int_vector<>(set.size());
            if(set.size() > 0){
                for(int64_t i = 0; i < set.size(); i++){
                    (*ptr)[i] = set[i];
                }
            }
            length = ptr->size();
            data_ptr = ptr; // Assign to variant
        }
    }

    ~Color_Set(){
        auto call_delete = [](auto ptr){delete ptr;};
        std::visit(call_delete, data_ptr);
    }

    bool empty() const {return colorset_is_empty(*this);};
    bool is_bitmap() const {return colorset_is_bitmap(*this);};
    int64_t size() const {return colorset_size(*this);}
    int64_t size_in_bits() const {return colorset_size_in_bits(*this);}
    bool contains(int64_t color) const {return colorset_contains(*this, color);}
    vector<int64_t> get_colors_as_vector() const {return colorset_get_colors_as_vector(*this);}

    // Stores the intersection back to to this object
    void intersection(const Color_Set_View& other){
        if(is_bitmap() && other.is_bitmap()){
            this->length = bitmap_vs_bitmap_intersection(*std::get<sdsl::bit_vector*>(data_ptr), this->length, *std::get<const sdsl::bit_vector*>(other.data_ptr), other.start, other.length);
        } else if(!is_bitmap() && other.is_bitmap()){
            this->length = array_vs_bitmap_intersection(*std::get<sdsl::int_vector<>*>(data_ptr), this->length, *std::get<const sdsl::bit_vector*>(other.data_ptr), other.start, other.length);
        } else if(is_bitmap() && !other.is_bitmap()){
            // The result will be sparse, so this will turn our representation into an array
            sdsl::int_vector<> iv_copy(*std::get<const sdsl::int_vector<>*>(other.data_ptr)); // Make a mutable copy

            // Intersect into the mutable copy
            int64_t iv_copy_length = array_vs_bitmap_intersection(iv_copy, other.length, *std::get<sdsl::bit_vector*>(this->data_ptr), this->start, this->length);

            // Replace our data with the mutable copy
            auto call_delete = [](auto ptr){delete ptr;}; // Free current data
            std::visit(call_delete, this->data_ptr);
            this->data_ptr = new sdsl::int_vector<>(iv_copy);
            this->length = iv_copy_length;
        } else{ // Array vs Array
            this->length = array_vs_array_intersection(*std::get<sdsl::int_vector<>*>(data_ptr), this->length, *std::get<const sdsl::int_vector<>*>(other.data_ptr), other.start, other.length);
        }
    }

    void do_union(const Color_Set_View& other){
        // TODO: DO PROPERLY
        vector<int64_t> A = this->get_colors_as_vector();
        vector<int64_t> B = other.get_colors_as_vector();
        vector<int64_t> AB(A.size() + B.size());
        int64_t len = union_buffers(A, A.size(), B, B.size(), AB);
        AB.resize(len);
        *this = Color_Set(AB);
    }

};

// Forward declaration of template
template<typename colorset_t, typename colorset_view_t>
requires Color_Set_Interface<colorset_t>
class Color_Set_Storage;

// Template specialization for Color_Set
template<>
class Color_Set_Storage<Color_Set, Color_Set_View>{

    private:

    sdsl::bit_vector bitmap_concat;
    sdsl::int_vector<> bitmap_starts; // bitmap_starts[i] = starting position of the i-th bitmap

    sdsl::int_vector<> arrays_concat;
    sdsl::int_vector<> arrays_starts; // arrays_starts[i] = starting position of the i-th subarray

    sdsl::bit_vector is_bitmap_marks;
    sdsl::rank_support_v5<> is_bitmap_marks_rs;

    // Dynamic-length vectors used during construction only
    // TODO: refactor these out of the class to a separate construction class
    vector<bool> temp_bitmap_concat;
    vector<int64_t> temp_arrays_concat;
    vector<int64_t> temp_bitmap_starts;
    vector<int64_t> temp_arrays_starts;
    vector<bool> temp_is_bitmap_marks;

    // Number of bits required to represent x
    int64_t bits_needed(uint64_t x){
        return max((int64_t)std::bit_width(x), (int64_t)1); // Need at least 1 bit (for zero)
    }

    sdsl::bit_vector to_sdsl_bit_vector(const vector<bool>& v){
        if(v.size() == 0) return sdsl::bit_vector();
        sdsl::bit_vector bv(v.size());
        for(int64_t i = 0; i < v.size(); i++) bv[i] = v[i];
        return bv;
    }

    sdsl::int_vector<> to_sdsl_int_vector(const vector<int64_t>& v){
        if(v.size() == 0) return sdsl::int_vector<>();
        int64_t max_element = *std::max_element(v.begin(), v.end());
        sdsl::int_vector iv(v.size(), 0, bits_needed(max_element));
        for(int64_t i = 0; i < v.size(); i++) iv[i] = v[i];
        return iv;
    }

    public:

    Color_Set_Storage() {}

    // Build from list of sets directly. Instead of using this constructor, you should probably just
    // call add_set for each set you want to add separately and then call prepare_for_queries when done.
    // This constructor is just to have the same interface as the other color set storage class.
    Color_Set_Storage(const vector<Color_Set>& sets){
        for(const Color_Set& cs : sets){
            add_set(cs.get_colors_as_vector());
        }
        prepare_for_queries();
    }

    Color_Set_View get_color_set_by_id(int64_t id) const{
        if(is_bitmap_marks[id]){
            int64_t bitmap_idx = is_bitmap_marks_rs.rank(id); // This many bitmaps come before this bitmap
            int64_t start = bitmap_starts[bitmap_idx];
            int64_t end = bitmap_starts[bitmap_idx+1]; // One past the end

            std::variant<const sdsl::bit_vector*, const sdsl::int_vector<>*> data_ptr = &bitmap_concat;
            return Color_Set_View(data_ptr, start, end-start);
        } else{
            int64_t arrays_idx = id - is_bitmap_marks_rs.rank(id); // Rank-0. This many arrays come before this bitmap
            int64_t start = arrays_starts[arrays_idx];
            int64_t end = arrays_starts[arrays_idx+1]; // One past the end

            std::variant<const sdsl::bit_vector*, const sdsl::int_vector<>*> data_ptr = &arrays_concat;
            return Color_Set_View(data_ptr, start, end-start);
        }
    }

    // Need to call prepare_for_queries() after all sets have been added
    // Set must be sorted
    void add_set(const vector<int64_t>& set){

        int64_t max_element = *std::max_element(set.begin(), set.end());
        if(log2(max_element) * set.size() > max_element){
            // Dense -> bitmap

            // Add is_bitmap_mark
            temp_is_bitmap_marks.push_back(1);

            // Store bitmap start
            temp_bitmap_starts.push_back(temp_bitmap_concat.size());

            // Create bitmap
            vector<bool> bitmap(max_element+1, 0);
            for(int64_t x : set) bitmap[x] = 1;
            for(bool b : bitmap) temp_bitmap_concat.push_back(b);

        } else{
            // Sparse -> Array

            // Add is_bitmap_mark
            temp_is_bitmap_marks.push_back(0);

            // Store array start
            temp_arrays_starts.push_back(temp_arrays_concat.size());

            if(set.size() > 0){
                for(int64_t i = 0; i < set.size(); i++){
                    temp_arrays_concat.push_back(set[i]);
                }
            }
        }

    }


    // Call this after done with add_set
    void prepare_for_queries(){

        // Add extra starts points one past the end
        // These eliminate a special case when querying for the size of the last color set
        temp_bitmap_starts.push_back(temp_bitmap_concat.size());
        temp_arrays_starts.push_back(temp_arrays_concat.size());

        arrays_concat = to_sdsl_int_vector(temp_arrays_concat);
        bitmap_starts = to_sdsl_int_vector(temp_bitmap_starts);
        arrays_starts = to_sdsl_int_vector(temp_arrays_starts);
        bitmap_concat = to_sdsl_bit_vector(temp_bitmap_concat);
        is_bitmap_marks = to_sdsl_bit_vector(temp_is_bitmap_marks);

        sdsl::util::init_support(is_bitmap_marks_rs, &is_bitmap_marks);

        // Free memory
        temp_arrays_concat.clear(); temp_arrays_concat.shrink_to_fit();    
        temp_bitmap_concat.clear(); temp_bitmap_concat.shrink_to_fit();
        temp_is_bitmap_marks.clear(); temp_is_bitmap_marks.shrink_to_fit();
        temp_arrays_starts.clear(); temp_arrays_starts.shrink_to_fit();
        temp_bitmap_starts.clear(); temp_bitmap_starts.shrink_to_fit();
    }

    int64_t serialize(ostream& os) const{
        int64_t bytes_written = 0;

        bytes_written += bitmap_concat.serialize(os);
        bytes_written += bitmap_starts.serialize(os);

        bytes_written += arrays_concat.serialize(os);
        bytes_written += arrays_starts.serialize(os);

        bytes_written += is_bitmap_marks.serialize(os);
        bytes_written += is_bitmap_marks_rs.serialize(os);

        return bytes_written;

        // Do not serialize temp structures
    }

    void load(istream& is){
        bitmap_concat.load(is);
        bitmap_starts.load(is);
        arrays_concat.load(is);
        arrays_starts.load(is);
        is_bitmap_marks.load(is);

        is_bitmap_marks_rs.load(is, &is_bitmap_marks);

        // Do not load temp structures
    }

    int64_t number_of_sets_stored() const{
        return is_bitmap_marks.size();
    }

    vector<Color_Set_View> get_all_sets() const{
        vector<Color_Set_View> all;
        for(int64_t i = 0; i < number_of_sets_stored(); i++){
            all.push_back(get_color_set_by_id(i));
        }
        return all;
    }

    // Returns map: component -> number of bytes
    map<string, int64_t> space_breakdown() const{
        map<string, int64_t> breakdown;

        sbwt::SeqIO::NullStream ns;

        breakdown["bitmaps-concat"] = bitmap_concat.serialize(ns);
        breakdown["bitmaps-starts"] = bitmap_starts.serialize(ns);
        breakdown["arrays-concat"] = arrays_concat.serialize(ns);
        breakdown["arrays-starts"] = arrays_starts.serialize(ns);
        breakdown["is-bitmap-marks"] = is_bitmap_marks.serialize(ns);
        breakdown["is-bitmap-marks-rank-suppport"] = is_bitmap_marks_rs.serialize(ns);

        // In the future maybe the space breakdown struct should support float statistics but for now we just disgustingly print to cout.
        cout << "Fraction of bitmaps in coloring: " << (double) is_bitmap_marks_rs.rank(is_bitmap_marks.size()) / is_bitmap_marks.size() << endl;

        return breakdown;
    }

};