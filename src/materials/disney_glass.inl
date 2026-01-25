#include "../microfacet.h"

Spectrum eval_op::operator()(const DisneyGlass &bsdf) const {
    bool reflect = dot(vertex.geometric_normal, dir_in) *
                   dot(vertex.geometric_normal, dir_out) > 0;
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // (void)reflect; // silence unuse warning, remove this when implementing hw

    // F_g = Fresnel term
    // F_g = 0.5 (R_s * R_s + R_p * R_p)
    // R_s = (dot(h, dir_in) - eta * dot(h, dir_out)) / (dot(h, dir_in) + eta * dot(h, dir_out))
    // R_p = (eta * dot(h, dir_in) - dot(h, dir_out)) / (eta * dot(h, dir_in) + dot(h, dir_out))

    Real eta = dot(vertex.geometric_normal, dir_in) > 0 ? bsdf.eta : 1 / bsdf.eta;
    Spectrum baseColor = eval(
        bsdf.base_color, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(
        bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Clamp roughness to avoid numerical issues.
    roughness_value = std::clamp(roughness_value, Real(0.01), Real(1));
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);

    Vector3 half_vector;
    if (reflect) {
        half_vector = normalize(dir_in + dir_out);
    } else {
        half_vector = normalize(dir_in + eta * dir_out);
    }

    // Flip half-vector if it's below surface
    if (dot(half_vector, frame.n) < 0) {
        half_vector = -half_vector;
    }

    Vector3 h_local = to_local(frame, half_vector);
    Vector3 wi_local = to_local(frame, dir_in);
    Vector3 wo_local = to_local(frame, dir_out);
    Real h_dot_in = dot(half_vector, dir_in);
    Real h_dot_out = dot(half_vector, dir_out);

    // F_g calculation
    Real R_s = (h_dot_in - eta * h_dot_out) / (h_dot_in + eta * h_dot_out);
    Real R_p = (eta * h_dot_in - h_dot_out) / (eta * h_dot_in + h_dot_out);
    Real F_g = Real(0.5) * (R_s * R_s + R_p * R_p);

    // D_g = Normal Distribution Function (GGX)
    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);
    Real half_vec_denom = (h_local.x * h_local.x) / (alpha_x * alpha_x) + (h_local.y * h_local.y) / (alpha_y * alpha_y) + (h_local.z * h_local.z);
    Real d_m = Real(1.0) / (M_PI * alpha_x * alpha_y * half_vec_denom * half_vec_denom);

    // g_in = 1 / (1 + lambda_sqrt_in)
    // lambda_sqrt_in = (sqrt(1 + ((dir_l.x * alpha_x)^2 + (dir_l.y * alpha_y)^2   /  dir_l.z^2)) -1) / 2
    Real lambda_sqrt_in = (sqrt(1 + ((wi_local.x * alpha_x) * (wi_local.x * alpha_x) + (wi_local.y * alpha_y) * (wi_local.y * alpha_y)) / (wi_local.z * wi_local.z)) - 1) / 2.0;
    Real g_in = 1.0 / (1.0 + lambda_sqrt_in);
    // g_out = 1 / (1 + lambda_sqrt_out)
    // lambda = (sqrt(1 + ((dir_l.x * alpha_x)^2 + (dir_l.y * alpha_y)^2   /  dir_l.z^2)) -1) / 2
    Real lambda_sqrt_out = (sqrt(1 + ((wo_local.x * alpha_x) * (wo_local.x * alpha_x) + (wo_local.y * alpha_y) * (wo_local.y * alpha_y)) / (wo_local.z * wo_local.z)) - 1) / 2.0;
    Real g_out = 1.0 / (1.0 + lambda_sqrt_out);
    // g_m = g_in * g_out
    Real g_m = g_in * g_out;
     
    Spectrum result;
    if (reflect) {
        // compute reflection contribution
        result = (baseColor * F_g * d_m * g_m) / (4.0 * abs(dot(frame.n, dir_in)));
    } else {
        // compute refraction contribution
        result = (sqrt(baseColor) * (1 - F_g) * d_m * g_m * abs(h_dot_out * h_dot_in)) 
                 / (abs(dot(frame.n, dir_in)) * (h_dot_in + eta * h_dot_out) * (h_dot_in + eta * h_dot_out));
    }

    return result;
}

Real pdf_sample_bsdf_op::operator()(const DisneyGlass &bsdf) const {
    bool reflect = dot(vertex.geometric_normal, dir_in) *
                   dot(vertex.geometric_normal, dir_out) > 0;
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    (void)reflect; // silence unuse warning, remove this when implementing hw

    return 0;
}

std::optional<BSDFSampleRecord>
        sample_bsdf_op::operator()(const DisneyGlass &bsdf) const {
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!

    return {};
}

TextureSpectrum get_texture_op::operator()(const DisneyGlass &bsdf) const {
    return bsdf.base_color;
}
