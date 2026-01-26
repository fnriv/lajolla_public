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
    Real subsurface_value = eval(bsdf.subsurface, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_value = eval(bsdf.specular, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real specular_tint_value = eval(bsdf.specular_tint, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real anisotropic_value = eval(bsdf.anisotropic, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real sheen_value = eval(bsdf.sheen, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real sheen_tint_value = eval(bsdf.sheen_tint, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real clearcoat_value = eval(bsdf.clearcoat, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real clearcoat_gloss_value = eval(bsdf.clearcoat_gloss, vertex.uv, vertex.uv_screen_size, texture_pool);
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

    // update f_metal to account for metallic factor

    // K_s = (1 - specular_tint) + specular_tint * C_tint
    Real luminance_value = luminance(base_color);
    Spectrum c_tint;
    if (luminance_value > 0) {
        c_tint = base_color / luminance_value;
    } else {
        c_tint = make_const_spectrum(1.0);
    }
    Real k_s = (Real(1.0) - specular_tint_value) + specular_tint_value * c_tint.x;

    // C_0 = specular * (R_0(eta) * (1 - metallic) * K_s + metallic * baseColor)
    Spectrum c_0 = specular_value * 
        (fresnel_dielectric(Real(0.0), eta) * (Real(1.0) - metallic_value) * k_s + metallic_value * base_color);

    // F_m_prime = c_0 + (1 - c_0) * (1 - abs(dot(h, dir_out)))^5
    Vector3 half_vector = normalize(dir_in + dir_out);
    Spectrum f_m_prime = c_0 + (make_const_spectrum(1.0) - c_0) * pow(Real(1.0) - abs(dot(half_vector, dir_out)), 5);

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
        (Real(1.0) - specular_transmission_value * (Real(1.0) - metallic_value)) * f_m_prime +
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
        Real glass = (*this)(DisneyGlass{
            bsdf.base_color, bsdf.roughness, bsdf.anisotropic, bsdf.eta});
        return (1 - metallic_value) * specular_transmission_value * glass;
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
        Real rnd = rnd_param_uv[0] * totalWeight;

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
