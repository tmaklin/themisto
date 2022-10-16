#include "new_coloring.hh"

void load_coloring(string filename, const plain_matrix_sbwt_t& SBWT, 
std::variant<
Coloring<Bitmap_Or_Deltas_ColorSet>, 
Coloring<Roaring_Color_Set>>& coloring){

    Coloring<Bitmap_Or_Deltas_ColorSet> coloring1;
    Coloring<Roaring_Color_Set> coloring2;

    try{    
        throwing_ifstream colors_in(filename, ios::binary);
        coloring = coloring1;
        std::get<Coloring<Bitmap_Or_Deltas_ColorSet>>(coloring).load(colors_in.stream, SBWT);    
        return; // No exception thrown
    } catch(Coloring<Bitmap_Or_Deltas_ColorSet>::WrongTemplateParameterException& e){
        // Was not this one
    }
    
    try{
        throwing_ifstream colors_in(filename, ios::binary);
        coloring = coloring2;
        std::get<Coloring<Roaring_Color_Set>>(coloring).load(colors_in.stream, SBWT);
        return; // No exception thrown
    } catch(Coloring<Roaring_Color_Set>::WrongTemplateParameterException& e){
        // Was not this one
    }

    throw std::runtime_error("Error: could not load color structure.");
}