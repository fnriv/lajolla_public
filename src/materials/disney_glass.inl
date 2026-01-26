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

    // F_g calculation - use fresnel_dielectric which handles Snell's law internally
    Real F_g = fresnel_dielectric(h_dot_in, eta);

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
        result = (baseColor * F_g * d_m * g_m) / (4.0 * fabs(dot(frame.n, dir_in)));
    } else {
        // compute refraction contribution
        // eta_factor accounts for the non-reciprocal nature of refraction
        Real eta_factor = dir == TransportDirection::TO_LIGHT ? (1 / (eta * eta)) : 1;
        Real sqrt_denom = h_dot_in + eta * h_dot_out;
        result = (sqrt(baseColor) * eta_factor * (1 - F_g) * d_m * g_m * eta * eta * fabs(h_dot_out * h_dot_in)) 
                 / (fabs(dot(frame.n, dir_in)) * sqrt_denom * sqrt_denom);
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
    // (void)reflect; // silence unuse warning, remove this when implementing hw
    // If we are going into the surface, then we use normal eta
    // (internal/external), otherwise we use external/internal.
    Real eta = dot(vertex.geometric_normal, dir_in) > 0 ? bsdf.eta : 1 / bsdf.eta;
    assert(eta > 0);

    Vector3 half_vector;
    if (reflect) {
        half_vector = normalize(dir_in + dir_out);
    } else {
        // "Generalized half-vector" from Walter et al.
        // See "Microfacet Models for Refraction through Rough Surfaces"
        half_vector = normalize(dir_in + dir_out * eta);
    }

    // Flip half-vector if it's below surface
    if (dot(half_vector, frame.n) < 0) {
        half_vector = -half_vector;
    }

    Real roughness_value = eval(
        bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Clamp roughness to avoid numerical issues.
    roughness_value = std::clamp(roughness_value, Real(0.01), Real(1));
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);

    // We sample the visible normals, also we use F to determine
    // whether to sample reflection or refraction
    // so PDF ~ F * D * G_in for reflection, PDF ~ (1 - F) * D * G_in for refraction.
    Real h_dot_in = dot(half_vector, dir_in);
    Vector3 h_local = to_local(frame, half_vector);
    Vector3 wi_local = to_local(frame, dir_in);
    Vector3 wo_local = to_local(frame, dir_out);
    Real F = fresnel_dielectric(h_dot_in, eta);
   // D_g = Normal Distribution Function (GGX)
    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);
    Real half_vec_denom = (h_local.x * h_local.x) / (alpha_x * alpha_x) + (h_local.y * h_local.y) / (alpha_y * alpha_y) + (h_local.z * h_local.z);
    Real D = Real(1.0) / (M_PI * alpha_x * alpha_y * half_vec_denom * half_vec_denom);

    // g_in = 1 / (1 + lambda_sqrt_in)
    // lambda_sqrt_in = (sqrt(1 + ((dir_l.x * alpha_x)^2 + (dir_l.y * alpha_y)^2   /  dir_l.z^2)) -1) / 2
    Real lambda_sqrt_in = (sqrt(1 + ((wi_local.x * alpha_x) * (wi_local.x * alpha_x) + (wi_local.y * alpha_y) * (wi_local.y * alpha_y)) / (wi_local.z * wi_local.z)) - 1) / 2.0;
    Real G_in = 1.0 / (1.0 + lambda_sqrt_in);

    if (reflect) {
        return (F * D * G_in) / (4 * fabs(dot(frame.n, dir_in)));
    } else {
        Real h_dot_out = dot(half_vector, dir_out);
        Real sqrt_denom = h_dot_in + eta * h_dot_out;
        Real dh_dout = eta * eta * h_dot_out / (sqrt_denom * sqrt_denom);
        return (1 - F) * D * G_in * fabs(dh_dout * h_dot_in / dot(frame.n, dir_in));
    }


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
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real eta = dot(vertex.geometric_normal, dir_in) > 0 ? bsdf.eta : 1 / bsdf.eta;

    Real roughness_value = eval(
        bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Clamp roughness to avoid numerical issues.
    roughness_value = std::clamp(roughness_value, Real(0.01), Real(1));

    // Compute anisotropic alpha values
    Real aspect = sqrt(1 - (anisotropic_value * 0.9));
    Real alpha_x = std::max(0.0001, (roughness_value * roughness_value) / aspect);
    Real alpha_y = std::max(0.0001, roughness_value * roughness_value * aspect);

    // Transform to local space first
    Vector3 local_dir_in = to_local(frame, dir_in);

    // Stretch incoming direction for anisotropic sampling
    Vector3 stretched_dir_in = normalize(Vector3{
        alpha_x * local_dir_in.x,
        alpha_y * local_dir_in.y,
        local_dir_in.z
    });

    // Sample micro normal in stretched (isotropic) space
    Vector3 local_micro_normal = sample_visible_normals(stretched_dir_in, Real(1), rnd_param_uv);

    Vector3 unstretched_micro_normal = normalize(Vector3{
        alpha_x * local_micro_normal.x,
        alpha_y * local_micro_normal.y,
        std::max(Real(0), local_micro_normal.z)
    });

    Vector3 half_vector = to_world(frame, unstretched_micro_normal);
    // Flip half-vector if it's below surface
    if (dot(half_vector, frame.n) < 0) {
        half_vector = -half_vector;
    }

    // Now we need to decide whether to reflect or refract.
    // We do this using the Fresnel term.
    Real h_dot_in = dot(half_vector, dir_in);
    Real F = fresnel_dielectric(h_dot_in, eta);

    if (rnd_param_w <= F) {
        // reflection
        Vector3 reflected = normalize(-dir_in + 2 * dot(dir_in, half_vector) * half_vector);
        // set eta to 0 since we are not transmitting
        return BSDFSampleRecord{reflected, Real(0) /* eta */, roughness_value};
    } else {
        // refraction
        // https://en.wikipedia.org/wiki/Snell%27s_law#Vector_form
        // (note that our eta is eta2 / eta1, and l = -dir_in)
        Real h_dot_out_sq = 1 - (1 - h_dot_in * h_dot_in) / (eta * eta);
        if (h_dot_out_sq <= 0) {
            // Total internal reflection
            // This shouldn't really happen, as F will be 1 in this case.
            return {};
        }
        // flip half_vector if needed
        if (h_dot_in < 0) {
            half_vector = -half_vector;
        }
        Real h_dot_out= sqrt(h_dot_out_sq);
        Vector3 refracted = -dir_in / eta + (fabs(h_dot_in) / eta - h_dot_out) * half_vector;
        return BSDFSampleRecord{refracted, eta, roughness_value};
    }
    // return {};
}

TextureSpectrum get_texture_op::operator()(const DisneyGlass &bsdf) const {
    return bsdf.base_color;
}
