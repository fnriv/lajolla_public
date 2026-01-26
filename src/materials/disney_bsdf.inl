#include "../microfacet.h"

Spectrum eval_op::operator()(const DisneyBSDF &bsdf) const {
    bool reflect = dot(vertex.geometric_normal, dir_in) *
                   dot(vertex.geometric_normal, dir_out) > 0;
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // (void)reflect; // silence unuse warning, remove this when implementing hw

    // access parameters
    Spectrum base_color = eval(bsdf.base_color, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_transmission_value = eval(bsdf.specular_transmission, vertex.uv
        , vertex.uv_screen_size, texture_pool);
    Real metallic_value = eval(bsdf.metallic, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Real subsurface_value = eval(bsdf.subsurface, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_value = eval(bsdf.specular, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_tint_value = eval(bsdf.specular_tint, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real sheen_value = eval(bsdf.sheen, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Real sheen_tint_value = eval(bsdf.sheen_tint, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real clearcoat_value = eval(bsdf.clearcoat, vertex.uv, vertex.uv_screen_size, texture_pool);
    // Real clearcoat_gloss_value = eval(bsdf.clearcoat_gloss, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real eta = bsdf.eta;

    // have to dereference to access the bsdf in the texture struct object things
    
    Spectrum f_diffuse = (*this)(DisneyDiffuse{
        bsdf.base_color, bsdf.subsurface, bsdf.roughness});
    Spectrum f_sheen = (*this)(DisneySheen{
        bsdf.base_color, bsdf.sheen_tint});
    Spectrum f_clearcoat = (*this)(DisneyClearcoat{
        bsdf.clearcoat_gloss});
    Spectrum f_glass = (*this)(DisneyGlass{
        bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
    Spectrum f_metal = (*this)(DisneyMetal{
        bsdf.base_color, bsdf.roughness, bsdf.anisotropic});

    Vector3 wi_local = to_local(frame, dir_in);
    Vector3 wo_local = to_local(frame, dir_out);

    // update f_metal to account for metallic factor
    Spectrum f_metal_prime;
    if (fabs(wi_local.z) < 1e-6 || fabs(wo_local.z) < 1e-6) {
        f_metal_prime = make_zero_spectrum();
    } else {
        // K_s = (1 - specular_tint) + specular_tint * C_tint
        Real luminance_value = luminance(base_color);
        Spectrum c_tint;
        if (luminance_value > 0) {
            c_tint = base_color / luminance_value;
        } else {
            c_tint = make_const_spectrum(1.0);
        }
        Spectrum k_s = (make_const_spectrum(1.0) - specular_tint_value) + specular_tint_value * c_tint;

        // C_0 = specular * (R_0(eta) * (1 - metallic) * K_s + metallic * baseColor)
        Spectrum c_0 = specular_value * 
            (fresnel_dielectric(Real(0.0), eta) * (Real(1.0) - metallic_value) * k_s + metallic_value * base_color);

        // F_m_prime = c_0 + (1 - c_0) * (1 - abs(dot(h, dir_out)))^5
        Vector3 half_vector = normalize(dir_in + dir_out);
        Spectrum f_m_prime = c_0 + (make_const_spectrum(1.0) - c_0) * pow(Real(1.0) - abs(dot(half_vector, dir_out)), 5);

        // recalculate f_metal with f_m_prime
        // Spectrum f_metal = f_m * d_m * g_m / (4.0 * abs(n * dir_in)) --> replace f_m with f_m_prime
        // reuse calculations from disney metal
        Vector3 h_local = to_local(frame, half_vector);
        Vector3 wi_local = to_local(frame, dir_in);
        Vector3 wo_local = to_local(frame, dir_out);
        // d_m = Normal Distribution Function (GGX)
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
            
        f_metal_prime = f_m_prime * d_m * g_m / (4.0 * abs(dot(frame.n, dir_in)));

    }
    

    // f_disney_bsdf = (1 - specular_transmission) * (1 - metallic) * f_diffuse +
    //                 (1 - metallic) * sheen * f_sheen +
    //                 (1 - specular_transmission * (1 - metallic)) * f_metal_prime +
    //                 Real(0.25) * clearcoat * f_clearcoat + 
    //                 (1 - metallic) specular_transmission * f_glass
    bool inside = dot(vertex.geometric_normal, dir_in) < 0;
    if (inside){
        // glass still relect and refract 
        return (1 - metallic_value) * specular_transmission_value * f_glass;
    }
    Spectrum f_disney_bsdf = 
        (Real(1.0) - specular_transmission_value) * (Real(1.0) - metallic_value) * f_diffuse +
        (Real(1.0) - metallic_value) * sheen_value * f_sheen +
        (Real(1.0) - specular_transmission_value * (Real(1.0) - metallic_value)) * f_metal_prime +
        Real(0.25) * clearcoat_value * f_clearcoat + 
        (Real(1.0) - metallic_value) * specular_transmission_value * f_glass;
    return f_disney_bsdf;
    // return make_zero_spectrum();
}

Real pdf_sample_bsdf_op::operator()(const DisneyBSDF &bsdf) const {
    bool reflect = dot(vertex.geometric_normal, dir_in) *
                   dot(vertex.geometric_normal, dir_out) > 0;
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // (void)reflect; // silence unuse warning, remove this when implementing hw
    bool inside = dot(vertex.geometric_normal, dir_in) < 0;

    // get params
    Real metallic_value = eval(bsdf.metallic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_transmission_value = eval(bsdf.specular_transmission, vertex.uv,
        vertex.uv_screen_size, texture_pool);
    Real clearcoat_value = eval(bsdf.clearcoat, vertex.uv, vertex.uv_screen_size, texture_pool);
    
    if (inside) {
        // glass still relect and refract 
        return (*this)(DisneyGlass{bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
    } else {
        
        // diffuseWeight = (1 - specular_transmission) * (1 - metallic)
        Real diffuseWeight = (1 - specular_transmission_value) * (1 - metallic_value);
        // metalWeight = (1 - specular_transmission * (1 - metallic))
        Real metalWeight = (1 - specular_transmission_value * (1 - metallic_value));
        // skip sheen!
        // clearcoatWeight = 0.25 * clearcoat
        Real clearcoatWeight = 0.25 * clearcoat_value;
        // glassWeight = (1 - metallic) * specular_transmission
        Real glassWeight = (1 - metallic_value) * specular_transmission_value;
        Real totalWeight = diffuseWeight + metalWeight + clearcoatWeight + glassWeight;
        Real pdf = 0;

        if (diffuseWeight > 0) {
            pdf += (diffuseWeight / totalWeight) * fmax(dot(frame.n, dir_out), Real(0)) / c_PI;
        }
        if (metalWeight > 0) {
            pdf += (metalWeight / totalWeight) * (*this)(DisneyMetal{
                bsdf.base_color, bsdf.roughness, bsdf.anisotropic});
        }
        if (clearcoatWeight > 0) {
            pdf += (clearcoatWeight / totalWeight) * (*this)(DisneyClearcoat{
                bsdf.clearcoat_gloss});
        }
        if (glassWeight > 0) {
            pdf += (glassWeight / totalWeight) * (*this)(DisneyGlass{
                bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
        }
        return pdf;
    }


    return 0;
}

std::optional<BSDFSampleRecord>
        sample_bsdf_op::operator()(const DisneyBSDF &bsdf) const {
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) * dot(vertex.geometric_normal, dir_in) < 0) {
        frame = -frame;
    }
    // Homework 1: implement this!
    // params 
    Real metallic_value = eval(bsdf.metallic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_transmission_value = eval(bsdf.specular_transmission, vertex.uv,
        vertex.uv_screen_size, texture_pool);
    Real clearcoat_value = eval(bsdf.clearcoat, vertex.uv, vertex.uv_screen_size, texture_pool);

    bool inside = dot(vertex.geometric_normal, dir_in) < 0;
    if (inside) {
        // glass only
        return (*this)(DisneyGlass{
            bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
    } else {
        // diffuseWeight = (1 - specular_transmission) * (1 - metallic)
        Real diffuseWeight = (1 - specular_transmission_value) * (1 - metallic_value);
        // metalWeight = (1 - specular_transmission * (1 - metallic))
        Real metalWeight = (1 - specular_transmission_value * (1 - metallic_value));
        // skip sheen!
        // clearcoatWeight = 0.25 * clearcoat
        Real clearcoatWeight = 0.25 * clearcoat_value;
        // glassWeight = (1 - metallic) * specular_transmission
        Real glassWeight = (1 - metallic_value) * specular_transmission_value;
        Real totalWeight = diffuseWeight + metalWeight + clearcoatWeight + glassWeight;

        if (totalWeight <= 0) {
            return {};
        }

        // scale 
        Real rnd = rnd_param_w * totalWeight;

        if (rnd < diffuseWeight) {
            return (*this)(DisneyDiffuse{
                bsdf.base_color, bsdf.subsurface, bsdf.roughness});
        } else if (rnd < diffuseWeight + metalWeight) {
            return (*this)(DisneyMetal{
                bsdf.base_color, bsdf.roughness, bsdf.anisotropic});
        } else if (rnd < diffuseWeight + metalWeight + clearcoatWeight) {
            return (*this)(DisneyClearcoat{
                bsdf.clearcoat_gloss});
        } else if (rnd < totalWeight) {
            return (*this)(DisneyGlass{
                bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
        }
    }
    
}

TextureSpectrum get_texture_op::operator()(const DisneyBSDF &bsdf) const {
    return bsdf.base_color;
}
