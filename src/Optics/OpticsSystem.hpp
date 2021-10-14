// Superclass for various optics system
//
// Specific optics systems should be derived from this class and should
// set the x and y parameters in their constructor to indicate the
// in focus area of the particular optic. This will allow the Renderer
// class to automatically determine the right viewing area for
// a paritcular Display + OpticsSystem (based on the display pixel
// density and optics system optimal viewing area).
//
// When deriving from this class, add the new class header to
// the file "AllOptics.hpp" so clients of these classes only
// need to include a single header to get all the different optics options.
//

#ifndef OPTICS_SYSTEM_HPP
#define OPTICS_SYSTEM_HPP

class OpticsSystem {
public:
    OpticsSystem();

    float get_x_inches();
    float get_y_inches();

    float get_x_mm();
    float get_y_mm();

    void set_eye_relief_mm(float new_eye_relief);
    void set_eye_relief_in(float new_eye_relief);

    void set_xy_in_inches(float new_x, float new_y);
    void set_xy_in_mm(float new_x, float new_y);

protected:
    float x_mm;
    float y_mm;
    float x_in;
    float y_in;
    float inch_mm_conversion = 25.4;
    float eye_relief_mm;
    float eye_relief_in;
};

#endif