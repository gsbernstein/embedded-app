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

    auto getXInches() -> float;
    auto getYInches() -> float;

    auto getXMm() -> float;
    auto getYMm() -> float;

    void setEyeReliefMm(float newEyeRelief);
    void setEyeReliefIn(float newEyeRelief);

    void setXyInInches(float newX, float newY);
    void setXyInMm(float newX, float newY);

protected:
    float xMm_;
    float yMm_;
    float xIn_;
    float yIn_;
    float inchMmConversion_ = 25.4;
    float eyeReliefMm_;
    float eyeReliefIn_;
};

#endif