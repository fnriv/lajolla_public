#include "../microfacet.h"

Spectrum eval_op::operator()(const DisneyMetal &bsdf) const {
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

    // access the parameters from the bsdf
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    Spectrum base_color = eval(bsdf.base_color, vertex.uv, vertex.uv_screen_size, texture_pool);

    Vector3 half_vector = normalize(dir_in + dir_out);
    Vector3 h_local = to_local(frame, half_vector);
    Vector3 wi_local = to_local(frame, dir_in);
    Vector3 wo_local = to_local(frame, dir_out);


    // f_m = Fresnel term
    // f_m = baseColor + (1 - baseColor)pow(1 - abs(dot(h, dirout)), 5)
    Spectrum f_m = base_color + (make_const_spectrum(1.0) - base_color) * pow(1 - abs(dot(half_vector, dir_out)), 5);


    // d_m = Normal Distribution Function (GGX)
    // d_m = 1 / (pi * alpha_x * alpha_y * half_vec_denom^2)
    // half_vec_denom = (h_local.x^2 / alpha_x^2) + (h_local.y^2 / alpha_y^2) + h_local.z^2

    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);
    Real half_vec_denom = (h_local.x * h_local.x) / (alpha_x * alpha_x) + (h_local.y * h_local.y) / (alpha_y * alpha_y) + (h_local.z * h_local.z);
    Real d_m = Real(1.0) / (M_PI * alpha_x * alpha_y * half_vec_denom * half_vec_denom);

    // g_in = 1 / (1 + lambda_sqrt_in)
    // lambda_sqrt_in = (sqrt(1 + ((dir_l.x * alpha_x)^2 + (dir_l.y * alpha_y)^2   /  dir_l.z^2)) -1) / 2
    Real lambda_sqrt_in = (sqrt(1 + ((wi_local.x * alpha_x) * (wi_local.x * alpha_x) + (wi_local.y * alpha_y) * (wi_local.y * alpha_y)) / (wi_local.z * wi_local.z)) - 1) / 2.0;
    Real g_in = 1.0 / (1.0 + lambda_sqrt_in);
    Real lambda_sqrt_out = (sqrt(1 + ((wo_local.x * alpha_x) * (wo_local.x * alpha_x) + (wo_local.y * alpha_y) * (wo_local.y * alpha_y)) / (wo_local.z * wo_local.z)) - 1) / 2.0;
    Real g_out = 1.0 / (1.0 + lambda_sqrt_out);
    Real g_m = g_in * g_out;

    // Spectrum f_metal = f_m * d_m * g_m / (4.0 * abs(n * dir_in))
    Spectrum f_metal = f_m * d_m * g_m / (4.0 * abs(dot(frame.n, dir_in)));
    return f_metal;
    
}

Real pdf_sample_bsdf_op::operator()(const DisneyMetal &bsdf) const {
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


    Vector3 half_vector = normalize(dir_in + dir_out);
    Real n_dot_in = dot(frame.n, dir_in);
    Real n_dot_out = dot(frame.n, dir_out);
    Real n_dot_h = dot(frame.n, half_vector);
    if (n_dot_out <= 0 || n_dot_h <= 0) {
        return 0;
    }

    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);

    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);

    Vector3 h_local = to_local(frame, half_vector);
    Real half_vec_denom = (h_local.x * h_local.x) / (alpha_x * alpha_x) +
                          (h_local.y * h_local.y) / (alpha_y * alpha_y) +
                          (h_local.z * h_local.z);
    Real d_m = Real(1.0) / (M_PI * alpha_x * alpha_y * half_vec_denom * half_vec_denom);

    Vector3 wi_local = to_local(frame, dir_in);
    Real lambda_sqrt_in = (sqrt(1 + ((wi_local.x * alpha_x) * (wi_local.x * alpha_x) +
                                     (wi_local.y * alpha_y) * (wi_local.y * alpha_y)) /
                                        (wi_local.z * wi_local.z)) - 1) / 2.0;
    Real g_in = 1.0 / (1.0 + lambda_sqrt_in);

    return (g_in * d_m) / (4.0 * abs(n_dot_in));
}

std::optional<BSDFSampleRecord>
        sample_bsdf_op::operator()(const DisneyMetal &bsdf) const {
    if (dot(vertex.geometric_normal, dir_in) < 0) {
        // No light below the surface
        return {};
    }
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) < 0) {
        frame = -frame;
    }

    // params
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);

    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);

    Vector3 local_dir_in = to_local(frame, dir_in);
    Vector3 stretched_dir_in = normalize(Vector3{
        alpha_x * local_dir_in.x,
        alpha_y * local_dir_in.y,
        local_dir_in.z
    });
    Vector3 local_micro_normal = sample_visible_normals(stretched_dir_in, Real(1), rnd_param_uv);
    Vector3 unstretched_micro_normal = normalize(Vector3{
        alpha_x * local_micro_normal.x,
        alpha_y * local_micro_normal.y,
        std::max(Real(0), local_micro_normal.z)
    });

    Vector3 half_vector = to_world(frame, unstretched_micro_normal);
    Vector3 reflected = normalize(-dir_in + 2 * dot(dir_in, half_vector) * half_vector);
    if (dot(frame.n, reflected) <= 0) {
        return {};
    }

    return BSDFSampleRecord{
        reflected,
        Real(0) /* eta */, roughness_value /* roughness */
    };
}

TextureSpectrum get_texture_op::operator()(const DisneyMetal &bsdf) const {
    return bsdf.base_color;
}
