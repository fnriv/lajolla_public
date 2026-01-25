#include "../microfacet.h"

Spectrum eval_op::operator()(const DisneyClearcoat &bsdf) const {
    if (dot(vertex.geometric_normal, dir_in) < 0 ||
            dot(vertex.geometric_normal, dir_out) < 0) {
        // No light below the surface
        return make_zero_spectrum();
    }
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!

    Real clearcoat_gloss_value = eval(bsdf.clearcoat_gloss, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real a_g = (Real(1.0) - clearcoat_gloss_value) * Real(0.1) + clearcoat_gloss_value * Real(0.001);
    Vector3 half_vector = normalize(dir_in + dir_out);
    Vector3 h_local = to_local(frame, half_vector);
    Vector3 wi_local = to_local(frame, dir_in);
    Vector3 wo_local = to_local(frame, dir_out);

    // F_clearcoat = Fresnel term
    // fresnel_c = r_0 + ((1 - r_0) * (1 - abs(dot(h, dir_out)))^5)
    // r_0(n) = ((n - 1)^2) / ((n + 1)^2)
        // use hard coded n = 1.5 for clearcoat
    Real r_0 = ((Real(1.5) - Real(1.0)) * (Real(1.5) - Real(1.0))) / ((Real(1.5) + Real(1.0)) * (Real(1.5) + Real(1.0)));
    Real fresnel_c = r_0 + ((1 - r_0) * std::pow(1 - std::abs(dot(half_vector, dir_out)), 5));

    // D_clearcoat = Normal Distribution Function (GGX)
    // d_c = (a_g^2 - 1) / (pi * log(a_g^2) * (1 + (a_g^2 - 1)*(h^l.z)^2))
    Real d_c = (a_g * a_g - 1) / (M_PI * log(a_g * a_g) * (1 + (a_g * a_g - 1) * (h_local.z * h_local.z)));

    // G_clearcoat = Geometry term (Smith GGX)
    // g_in = 1 / (1 + lambda_sqrt_in)
    // lambda_sqrt_in = (sqrt(1 + ((dir_l.x * 0.25)^2 + (dir_l.y * 0.25)^2   /  dir_l.z^2)) -1) / 2
    Real lambda_sqrt_in = (sqrt(1 + ((wi_local.x * 0.25) * (wi_local.x * 0.25) +
                                     (wi_local.y * 0.25) * (wi_local.y * 0.25)) /
                                        (wi_local.z * wi_local.z)) - 1) / 2.0;
    Real g_in = 1.0 / (1.0 + lambda_sqrt_in);
    // g_out = 1 / (1 + lambda_sqrt_out)
    // lambda_sqrt_out = (sqrt(1 + ((dir_v.x * 0.25)^2 + (dir_v.y * 0.25)^2   /  dir_v.z^2)) -1) / 2
    Real lambda_sqrt_out = (sqrt(1 + ((wo_local.x * 0.25) * (wo_local.x * 0.25) +
                                      (wo_local.y * 0.25) * (wo_local.y * 0.25)) /
                                         (wo_local.z * wo_local.z)) - 1) / 2.0;
    Real g_out = 1.0 / (1.0 + lambda_sqrt_out);
    Real g_c = g_in * g_out;

    // f_clearcoat = (F_clearcoat * D_clearcoat * G_clearcoat) / (4.0 * abs(n * dir_in))
    Spectrum f_clearcoat = make_const_spectrum(fresnel_c * d_c * g_c / (Real(4.0) * std::abs(dot(frame.n, dir_in))));
    return f_clearcoat;
}

Real pdf_sample_bsdf_op::operator()(const DisneyClearcoat &bsdf) const {
    if (dot(vertex.geometric_normal, dir_in) < 0 ||
            dot(vertex.geometric_normal, dir_out) < 0) {
        // No light below the surface
        return 0;
    }
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // PDF computes probability from known dir_in and dir_out (no random numbers here)

    Vector3 half_vector = normalize(dir_in + dir_out);
    Vector3 h_local = to_local(frame, half_vector);
    
    Real n_dot_h = dot(frame.n, half_vector);
    Real n_dot_out = dot(frame.n, dir_out);
    if (n_dot_out <= 0 || n_dot_h <= 0) {
        return 0;
    }

    Real clearcoat_gloss_value = eval(bsdf.clearcoat_gloss, vertex.uv, vertex.uv_screen_size, texture_pool); 
    Real alpha_g = (Real(1.0) - clearcoat_gloss_value) * Real(0.1) + clearcoat_gloss_value * Real(0.001);

    // D_clearcoat
    Real d_c = (alpha_g * alpha_g - 1) / (M_PI * log(alpha_g * alpha_g) * (1 + (alpha_g * alpha_g - 1) * (h_local.z * h_local.z)));

    // pdf = D_c * |n · h| / (4 * |h · dir_out|)
    Real h_dot_out = dot(half_vector, dir_out);
    return d_c * std::abs(n_dot_h) / (Real(4.0) * std::abs(h_dot_out));
    
}

std::optional<BSDFSampleRecord>
        sample_bsdf_op::operator()(const DisneyClearcoat &bsdf) const {
    if (dot(vertex.geometric_normal, dir_in) < 0) {
        // No light below the surface
        return {};
    }
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // Sample micronormal proportional to D_c then reflect dir_in

    Real clearcoat_gloss_value = eval(bsdf.clearcoat_gloss, vertex.uv, vertex.uv_screen_size, texture_pool); 
    Real alpha_g = (Real(1.0) - clearcoat_gloss_value) * Real(0.1) + clearcoat_gloss_value * Real(0.001);

    // Use the random numbers provided
    Real u0 = rnd_param_uv[0];
    Real u1 = rnd_param_uv[1];

    // cos(h_elevation) = sqrt((1 - (alpha^2)^(1-u0)) / (1 - alpha^2))
    // h_azimuth = 2*pi * u1
    Real cos_h_elevation = sqrt((Real(1.0) - pow(alpha_g * alpha_g, Real(1.0) - u0)) / (Real(1.0) - alpha_g * alpha_g));
    Real sin_h_elevation = sqrt(Real(1.0) - cos_h_elevation * cos_h_elevation);
    Real h_azimuth = Real(2.0) * M_PI * u1;

    Vector3 h_local = Vector3{
        sin_h_elevation * cos(h_azimuth),
        sin_h_elevation * sin(h_azimuth),
        cos_h_elevation
    };
    
    Vector3 half_vector = to_world(frame, h_local);
    Vector3 reflected = normalize(-dir_in + 2 * dot(dir_in, half_vector) * half_vector);
    
    if (dot(frame.n, reflected) <= 0) {
        return {};
    }

    return BSDFSampleRecord{
        reflected,
        Real(0) /* eta */, alpha_g /* roughness */
    };
}

TextureSpectrum get_texture_op::operator()(const DisneyClearcoat &bsdf) const {
    return make_constant_spectrum_texture(make_zero_spectrum());
}
