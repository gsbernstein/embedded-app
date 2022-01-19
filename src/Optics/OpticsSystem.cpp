#include "OpticsSystem.hpp"

OpticsSystem::OpticsSystem() { setXyInInches(0.0, 0.0); }

auto OpticsSystem::getXInches() -> float { return xIn_; }

auto OpticsSystem::getYInches() -> float { return yIn_; }

auto OpticsSystem::getXMm() -> float { return xMm_; }

auto OpticsSystem::getYMm() -> float { return yMm_; }

void OpticsSystem::setXyInInches(float newX, float newY) {
    xIn_ = newX;
    yIn_ = newY;
    xMm_ = xIn_ * inchMmConversion_;
    yMm_ = yIn_ * inchMmConversion_;
}

void OpticsSystem::setXyInMm(float newX, float newY) {
    xMm_ = newX;
    yMm_ = newY;
    xIn_ = xMm_ / inchMmConversion_;
    yIn_ = yMm_ / inchMmConversion_;
}

void OpticsSystem::setEyeReliefIn(float newEyeRelief) {
    eyeReliefIn_ = newEyeRelief;
    eyeReliefMm_ = eyeReliefIn_ * inchMmConversion_;
}

void OpticsSystem::setEyeReliefMm(float newEyeRelief) {
    eyeReliefMm_ = newEyeRelief;
    eyeReliefIn_ = eyeReliefMm_ / inchMmConversion_;
}
