Spectrum eval_op::operator()(const DisneyDiffuse &bsdf) const {
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

    Real subsurface_value = eval(bsdf.subsurface, vertex.uv, vertex.uv_screen_size, texture_pool);
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    Spectrum base_color = eval(bsdf.base_color, vertex.uv, vertex.uv_screen_size, texture_pool);
    
    Real cos_theta_in = dot(frame.n, dir_in);
    Real cos_theta_out = dot(frame.n, dir_out);

    Real fresnel_diffuse90 = 0.5 + 2.0 * roughness_value * cos_theta_out * cos_theta_out;

    Real fresnel_diffuse_in =
        Real(1) + (fresnel_diffuse90 - Real(1)) * pow((Real(1) - cos_theta_in), 5);
    Real fresnel_diffuse_out = 
        Real(1) + (fresnel_diffuse90 - Real(1)) * pow((Real(1) - cos_theta_out), 5);

    Spectrum f_base_diffuse = 
        (base_color / Real(M_PI)) * fresnel_diffuse_in * fresnel_diffuse_out * abs(cos_theta_out);

    Real fresnel_subsurface90 = roughness_value * cos_theta_out * cos_theta_out;

    Real fresnel_subsurface_in = 
        Real(1) + (fresnel_subsurface90 - Real(1)) * pow((Real(1) - cos_theta_in), 5);
    Real fresnel_subsurface_out = 
        Real(1) + (fresnel_subsurface90 - Real(1)) * pow((Real(1) - cos_theta_out), 5);
    
    Vector3 f_subsurface = 
        (Real(1.25) * base_color / Real(M_PI)) * 
        ((fresnel_subsurface_in * fresnel_subsurface_out * 
        ((1.0 / (cos_theta_in + cos_theta_out)) - Real(0.5))) + Real(0.5)) *
        abs(cos_theta_out);

    //f_diffuse = (1-subsurf) dot f_baseDiffuse + subsurface * f_subsurface
    Spectrum f_diffuse = 
        ((1.0 - subsurface_value) * f_base_diffuse) + (subsurface_value * f_subsurface);
    
    return f_diffuse;
}

Real pdf_sample_bsdf_op::operator()(const DisneyDiffuse &bsdf) const {
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
    // cosine hemisphere sampling
    return fmax(dot(frame.n, dir_out), Real(0)) / c_PI;
}

std::optional<BSDFSampleRecord> sample_bsdf_op::operator()(const DisneyDiffuse &bsdf) const {
    if (dot(vertex.geometric_normal, dir_in) < 0) {
        // No light below the surface
        return {};
    }
    // Flip the shading frame if it is inconsistent with the geometry normal
    Frame frame = vertex.shading_frame;
    if (dot(frame.n, dir_in) < 0) {
        frame = -frame;
    }
    Real roughness_value = eval(bsdf.roughness, vertex.uv, vertex.uv_screen_size, texture_pool);
    
    // Homework 1: implement this!
    return BSDFSampleRecord{
        to_world(frame, sample_cos_hemisphere(rnd_param_uv)),
        Real(0) /* eta */, roughness_value /* roughness */};
}

TextureSpectrum get_texture_op::operator()(const DisneyDiffuse &bsdf) const {
    return bsdf.base_color;
}
