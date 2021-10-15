#include "OpticsSystem.hpp"

OpticsSystem::OpticsSystem() { set_xy_in_inches(0.0, 0.0); }

auto OpticsSystem::get_x_inches() -> float { return x_in; }

auto OpticsSystem::get_y_inches() -> float { return y_in; }

auto OpticsSystem::get_x_mm() -> float { return x_mm; }

auto OpticsSystem::get_y_mm() -> float { return y_mm; }

void OpticsSystem::set_xy_in_inches(float new_x, float new_y) {
    x_in = new_x;
    y_in = new_y;
    x_mm = x_in * inch_mm_conversion;
    y_mm = y_in * inch_mm_conversion;
}

void OpticsSystem::set_xy_in_mm(float new_x, float new_y) {
    x_mm = new_x;
    y_mm = new_y;
    x_in = x_mm / inch_mm_conversion;
    y_in = y_mm / inch_mm_conversion;
}

void OpticsSystem::set_eye_relief_in(float new_eye_relief) {
    eye_relief_in = new_eye_relief;
    eye_relief_mm = eye_relief_in * inch_mm_conversion;
}

void OpticsSystem::set_eye_relief_mm(float new_eye_relief) {
    eye_relief_mm = new_eye_relief;
    eye_relief_in = eye_relief_mm / inch_mm_conversion;
}
