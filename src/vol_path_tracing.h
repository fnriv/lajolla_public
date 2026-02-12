#pragma once

#include <optional>
#include <cmath>

// The simplest volumetric renderer: 
// single absorption only homogeneous volume
// only handle directly visible light sources
Spectrum vol_path_tracing_1(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {

    int w = scene.camera.width, h = scene.camera.height;
    Vector2 screen_pos((x + next_pcg32_real<Real>(rng)) / w,
                      (y + next_pcg32_real<Real>(rng)) / h);
    Ray ray = sample_primary(scene.camera, screen_pos);
    RayDifferential ray_diff = init_ray_differential(w, h);


    std::optional<PathVertex> isect = intersect(scene, ray, ray_diff);
    
    if (isect) {
        int medium_id = scene.camera.medium_id;

        Spectrum transmittance = make_const_spectrum(1);

        const Medium &medium = scene.media[medium_id];  
        Spectrum sigma_a = get_sigma_a(medium, isect->position);
        
        Real t_hit = distance(ray.org, isect->position);
        transmittance = exp(-sigma_a * t_hit);

        Spectrum Le = make_zero_spectrum();
        if (is_light(scene.shapes[isect->shape_id])) {
            Le = emission(*isect, -ray.dir, scene);
        }
        return transmittance * Le;
    }
    return make_zero_spectrum();
}

// The second simplest volumetric renderer: 
// single monochromatic homogeneous volume with single scattering,
// no need to handle surface lighting, only directly visible light source
Spectrum vol_path_tracing_2(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {

    int w = scene.camera.width, h = scene.camera.height;
    Vector2 screen_pos((x + next_pcg32_real<Real>(rng)) / w,
                      (y + next_pcg32_real<Real>(rng)) / h);
    Ray ray = sample_primary(scene.camera, screen_pos);
    RayDifferential ray_diff = init_ray_differential(w, h);


    std::optional<PathVertex> isect = intersect(scene, ray, ray_diff);
    Real t_hit = infinity<Real>();

    if (isect) {
        t_hit = distance(ray.org, isect->position);
    }
    

    Spectrum sigma_a = get_sigma_a(scene.media[scene.camera.medium_id], isect->position);
    Spectrum sigma_s = get_sigma_s(scene.media[scene.camera.medium_id], isect->position);
    
    Spectrum sigma_t = sigma_a + sigma_s;

    Real u = next_pcg32_real<Real>(rng);
    Real t = -log(1 - u) / sigma_t.x; // assume monochromatic

    
    int medium_id = scene.camera.medium_id;

    if (medium_id < 0) {
        if (is_light(scene.shapes[isect->shape_id])) {
            return emission(*isect, -ray.dir, scene);
        }
        return make_zero_spectrum();
    }

    if (t < t_hit) {
        Spectrum trans_pdf = exp(-sigma_t * t) * sigma_t;
        Spectrum transmittance = exp(-sigma_t * t);

        // L_s1 using MC sampling
        Vector3 p = ray.org + t * ray.dir;
        
        Real light_w = next_pcg32_real<Real>(rng);
        int light_id = sample_light(scene, light_w);
        const Light &light = scene.lights[light_id];
        
        Vector2 light_uv{next_pcg32_real<Real>(rng), next_pcg32_real<Real>(rng)};
        PointAndNormal point_on_light = sample_point_on_light(
            light, p, light_uv, light_w, scene
        );

        Vector3 p_to_light = point_on_light.position - p;
        Real dist_to_light = length(p_to_light);
        Vector3 omega_prime = p_to_light / dist_to_light;  
        
        Ray shadow_ray{p, omega_prime, 1e-4f, dist_to_light - 1e-4f};
        bool visible = !occluded(scene, shadow_ray);

        if (visible) {
            
            // phase function: ρ(p, incoming_dir, outgoing_dir)
            PhaseFunction phase = get_phase_function(scene.media[medium_id]);
            Real phase_val = avg(eval(phase, -ray.dir, omega_prime));
            
            // emission from light: Le(p', ω')
            PathVertex light_vertex;
            light_vertex.position = point_on_light.position;
            light_vertex.geometric_normal = point_on_light.normal;
            light_vertex.shape_id = std::get<DiffuseAreaLight>(light).shape_id;
            Spectrum Le = emission(light_vertex, -omega_prime, scene);
            
            // transmittance from p to light: exp(-σt * distance)
            Spectrum transmittance_to_light = exp(-sigma_t * dist_to_light);
            
            // geometry term: |ω' · n| / distance²
            Real cos_theta = abs(dot(omega_prime, point_on_light.normal));
            Real geometry_term = cos_theta / (dist_to_light * dist_to_light);
            
            // PDF of sampling this light point
            Real pdf_light = light_pmf(scene, light_id) * 
                        pdf_point_on_light(light, point_on_light, p, scene);
            
            // MC estimate of L_scatter1
            Spectrum L_scatter1 = phase_val * Le * transmittance_to_light * 
                                geometry_term / pdf_light;
            
            return (transmittance / trans_pdf) * sigma_s * L_scatter1;
        }
        
        return make_zero_spectrum();

    } else {
        // surface emission
        Spectrum trans_pdf = exp(-sigma_t * t_hit);
        Spectrum transmittance = exp(-sigma_t * t_hit);
        Spectrum Le = make_zero_spectrum();
        if (is_light(scene.shapes[isect->shape_id])) {
            Le = emission(*isect, -ray.dir, scene);
        }
        // if (trans_pdf < 0) 
        return (transmittance / trans_pdf) * Le;
    }
    
    return make_zero_spectrum();
}


Medium update_medium(const PathVertex &isect, const Ray &ray, 
                    Medium &current_medium, const Scene &scene) {
    // updates the current medium that the ray is in

    if (isect.interior_medium_id != isect.exterior_medium_id) {
        if (dot(ray.dir, isect.geometric_normal) > 0) {
            // we are exiting the medium, update to exterior medium
            current_medium = scene.media[isect.exterior_medium_id];
        } else {
            // we are entering the medium, update to interior medium
            current_medium = scene.media[isect.interior_medium_id];
        }
    }
    return current_medium;  
}

// The third volumetric renderer (not so simple anymore): 
// multiple monochromatic homogeneous volumes with multiple scattering
// no need to handle surface lighting, only directly visible light source
Spectrum vol_path_tracing_3(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {

    int w = scene.camera.width, h = scene.camera.height;
    Vector2 screen_pos((x + next_pcg32_real<Real>(rng)) / w,
                      (y + next_pcg32_real<Real>(rng)) / h);
    Ray ray = sample_primary(scene.camera, screen_pos);
    RayDifferential ray_diff = init_ray_differential(w, h);

    Medium current_medium = scene.media[scene.camera.medium_id];

    Spectrum current_path_throughput = make_const_spectrum(1);

    Spectrum radiance = make_zero_spectrum();

    int bounces = 0;

    while (true) {

        bool scatter = false;

        std::optional<PathVertex> isect = intersect(scene, ray, ray_diff);

        Spectrum transmittance = make_const_spectrum(1);

        Spectrum trans_pdf = make_const_spectrum(1);

        Spectrum sigma_a = get_sigma_a(current_medium, ray.org);
        Spectrum sigma_s = get_sigma_s(current_medium, ray.org);
        Spectrum sigma_t = sigma_a + sigma_s;

        Real u = next_pcg32_real<Real>(rng);
        Real t = -log(1 - u) / sigma_t.x; // assume monochromatic 

        if (current_medium.index() == 0 || current_medium.index() == 1) { // check if we are in a medium

            Real t_hit = infinity<Real>();
            if (isect) {
                t_hit = distance(ray.org, isect->position);
            } 

            trans_pdf = exp(-sigma_t * t) * sigma_t;
            transmittance = exp(-sigma_t * t);

            if (t < t_hit) {
                scatter = true;
                ray.org = ray.org + t * ray.dir; 
            } 

        }

        current_path_throughput *= (transmittance / trans_pdf);

        if (!scatter) {
            if (isect && is_light(scene.shapes[isect->shape_id])) {
                radiance += current_path_throughput * emission(*isect, -ray.dir, scene);
            }
        }

        if (bounces == scene.options.max_depth - 1 && scene.options.max_depth != -1) {
            break;
        }

        if (!scatter && isect) {
            if (isect->material_id == -1) {
                current_medium = update_medium(*isect, ray, current_medium, scene);
                ray.org = isect->position + ray.dir * Real(1e-4);  
                bounces += 1;
                continue;
            } else {
                break;
            }
        }

        if (scatter) {
            PhaseFunction phase = get_phase_function(current_medium);
            Vector2 rnd_param{next_pcg32_real<Real>(rng), next_pcg32_real<Real>(rng)};
            std::optional<Vector3> next_dir_opt = sample_phase_function(phase, -ray.dir, rnd_param);
            Vector3 next_dir = next_dir_opt.value();
            Real phase_val = avg(eval(phase, -ray.dir, next_dir));
            Real pdf_phase = pdf_sample_phase(phase, -ray.dir, next_dir);
            current_path_throughput *= (phase_val / pdf_phase) * sigma_s;

            ray.dir = next_dir;
        } else {
            break; // hit surface, break for now since we don't handle surface lighting yet
        }

        // russian roulette! 
        Real rr_prob = 1;
        if (bounces >= scene.options.rr_depth) {
            rr_prob = std::min(avg(current_path_throughput), Real(0.95));
            if (next_pcg32_real<Real>(rng) > rr_prob) {
                break;
            } else {
                current_path_throughput /= rr_prob;
            }
        }

        bounces += 1;
    
    }
    return radiance;
}


Spectrum next_event_estimation(const Scene &scene, const Vector3 &p, const Vector3 &dir_in, const Medium &medium, pcg32_state &rng) {
    // sample light
    Real light_w = next_pcg32_real<Real>(rng);
    int light_id = sample_light(scene, light_w);
    const Light &light = scene.lights[light_id];
    
    // sample point on sampled light
    PointAndNormal point_on_light = sample_point_on_light(
        light, p,  
        Vector2{next_pcg32_real<Real>(rng), next_pcg32_real<Real>(rng)}, 
        light_w, 
        scene
    );
    
    Vector3 p_prime = point_on_light.position;
    Vector3 dir_to_light = p_prime - p;
    Real dist_to_light = length(dir_to_light);
    Vector3 omega_prime = dir_to_light / dist_to_light;
    
    // Trace shadow ray
    Spectrum T_light = make_const_spectrum(1);
    Medium shadow_medium = medium;
    Vector3 shadow_p = p;
    
    while (true) {
        Vector3 to_target = p_prime - shadow_p;
        Real remaining_dist = length(to_target);
        Vector3 shadow_dir = to_target / remaining_dist;
        
        Ray shadow_ray{shadow_p, shadow_dir, Real(1e-4), remaining_dist - Real(1e-4)};
        std::optional<PathVertex> isect = intersect(scene, shadow_ray);
        
        Real next_t = remaining_dist;
        if (isect) {
            next_t = distance(shadow_p, isect->position);
        }
        
        if (shadow_medium.index() == 0 || shadow_medium.index() == 1) {
            Spectrum sigma_a = get_sigma_a(shadow_medium, shadow_p);
            Spectrum sigma_s = get_sigma_s(shadow_medium, shadow_p);
            Spectrum sigma_t = sigma_a + sigma_s;
            T_light *= exp(-sigma_t * next_t);
        }
        
        if (!isect) { // hit light 
            break;
        }
        
        if (isect->material_id >= 0) {
            // Blocked by opaque surface
            return make_zero_spectrum();
        }
        
        shadow_medium = update_medium(*isect, shadow_ray, shadow_medium, scene);
        shadow_p = isect->position + shadow_dir * Real(1e-4);
    }
    
    if (avg(T_light) > 0) {

        Real cos_theta = abs(dot(omega_prime, point_on_light.normal));
        Real G = cos_theta / (dist_to_light * dist_to_light);
        
        PhaseFunction phase = get_phase_function(medium);
        Real phase_val = avg(eval(phase, dir_in, omega_prime)); 
        
        PathVertex light_vertex;
        light_vertex.position = point_on_light.position;
        light_vertex.geometric_normal = point_on_light.normal;
        light_vertex.shape_id = std::get<DiffuseAreaLight>(light).shape_id;  
        Spectrum Le = emission(light_vertex, -omega_prime, scene);
        
        Real pdf_nee = light_pmf(scene, light_id) *  
                      pdf_point_on_light(light, point_on_light, p, scene); 
        
        // Return raw contribution (NO MIS!)
        return phase_val * Le * T_light * G / pdf_nee;
    }
    
    return make_zero_spectrum();
}

// The fourth volumetric renderer: 
// multiple monochromatic homogeneous volumes with multiple scattering
// with MIS between next event estimation and phase function sampling
// still no surface lighting
Spectrum vol_path_tracing_4(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {
    // Homework 2: implememt this!

    int w = scene.camera.width, h = scene.camera.height;
    Vector2 screen_pos((x + next_pcg32_real<Real>(rng)) / w,
                    (y + next_pcg32_real<Real>(rng)) / h);
    Ray ray = sample_primary(scene.camera, screen_pos);
    RayDifferential ray_diff = RayDifferential{Real(0), Real(0)};

    Medium current_medium = scene.media[scene.camera.medium_id];
    Spectrum current_path_throughput = make_const_spectrum(1);
    Spectrum radiance = make_zero_spectrum();
    int bounces = 0;

    // MIS cache variables 
    Vector3 nee_p_cache = ray.org;        
    Real dir_pdf = 0;                     
    Real multi_trans_pdf = 1;             
    bool never_scatter = true;            

    while (true) {
        bool scatter = false;
        std::optional<PathVertex> isect = intersect(scene, ray, ray_diff);
        
        Spectrum transmittance = make_const_spectrum(1);
        Spectrum trans_pdf = make_const_spectrum(1);

        Spectrum sigma_a = get_sigma_a(current_medium, ray.org);
        Spectrum sigma_s = get_sigma_s(current_medium, ray.org);
        Spectrum sigma_t = sigma_a + sigma_s;

        Real u = next_pcg32_real<Real>(rng);
        Real t = -log(1 - u) / sigma_t.x;
        
        Real t_hit = infinity<Real>();

        if (current_medium.index() == 0 || current_medium.index() == 1) {
            if (isect) {
                t_hit = distance(ray.org, isect->position);
            }

            if (t < t_hit) {
                scatter = true;
                trans_pdf = exp(-sigma_t * t) * sigma_t;
                transmittance = exp(-sigma_t * t);
                ray.org = ray.org + t * ray.dir;
            } else if (isect) {
                // Hit surface
                trans_pdf = exp(-sigma_t * t_hit);
                transmittance = exp(-sigma_t * t_hit);
            }
        }

        if (bounces == scene.options.max_depth - 1 && scene.options.max_depth != -1) {
            break;
        }

        current_path_throughput *= (transmittance / trans_pdf);

        if (!scatter) {
            // Hit surface
            if (isect && is_light(scene.shapes[isect->shape_id])) {
                if (never_scatter) {
                    radiance += current_path_throughput * emission(*isect, -ray.dir, scene);
                } else {
                    // Use MIS
                    int light_id = get_area_light_id(scene.shapes[isect->shape_id]);
                    const Light &light = scene.lights[light_id];
                    PointAndNormal light_point{isect->position, isect->geometric_normal};
                    
                    Real pdf_nee = light_pmf(scene, light_id) * 
                                pdf_point_on_light(light, light_point, nee_p_cache, scene);
                    
                    // Geometry term
                    Vector3 dir_to_light = isect->position - nee_p_cache;
                    Real dist = length(dir_to_light);
                    Vector3 omega = dir_to_light / dist;
                    Real cos_theta = abs(dot(omega, isect->geometric_normal));
                    Real G = cos_theta / (dist * dist);
                    
                    // phase sampling pdf in area measure
                    Real dir_pdf_area = dir_pdf * multi_trans_pdf * G;
                    Real w = (dir_pdf_area * dir_pdf_area) / 
                            (dir_pdf_area * dir_pdf_area + pdf_nee * pdf_nee);
                    
                    radiance += current_path_throughput * emission(*isect, -ray.dir, scene) * w;
                }
            }

            // Handle surfaces
            if (isect) {
                if (isect->material_id == -1) {
                    // Index-matching
                    current_medium = update_medium(*isect, ray, current_medium, scene);
                    
                    // Update multi_trans_pdf BEFORE moving
                    Spectrum sigma_t_surface = get_sigma_a(current_medium, ray.org) + 
                                            get_sigma_s(current_medium, ray.org);
                    multi_trans_pdf *= avg(exp(-sigma_t_surface * t_hit));
                    
                    ray.org = isect->position + ray.dir * Real(1e-4);
                    bounces += 1;
                    continue;
                } else {
                    // Opaque surface - terminate
                    break;
                }
            } else {
                // No intersection - terminate
                break;
            }
        }

        if (scatter) {
            Vector3 p = ray.org;
            
            // NEE
            Spectrum nee_contrib = next_event_estimation(
                    scene, 
                p,
                -ray.dir,      // ADD THIS: incoming direction
                current_medium, 
                rng
            );
            radiance += current_path_throughput * sigma_s * nee_contrib;
            
            // Cache for MIS
            nee_p_cache = p;              
            multi_trans_pdf = 1;         
            never_scatter = false;        
            
            // Phase function sampling
            PhaseFunction phase = get_phase_function(current_medium);
            Vector2 rnd_param{next_pcg32_real<Real>(rng), next_pcg32_real<Real>(rng)};
            std::optional<Vector3> next_dir_opt = sample_phase_function(phase, -ray.dir, rnd_param);
            Vector3 next_dir = next_dir_opt.value();
            
            Real phase_val = avg(eval(phase, -ray.dir, next_dir));
            dir_pdf = pdf_sample_phase(phase, -ray.dir, next_dir);
            current_path_throughput *= (phase_val / dir_pdf) * sigma_s;
            
            ray.dir = next_dir;
        }

        // russian roulette
        Real rr_prob = 1;
        if (bounces >= scene.options.rr_depth) {
            rr_prob = std::min(avg(current_path_throughput), Real(0.95));
            if (next_pcg32_real<Real>(rng) > rr_prob) {
                break;
            }
            current_path_throughput /= rr_prob;
        }

        bounces += 1;
    }

    return radiance;
}

// The fifth volumetric renderer: 
// multiple monochromatic homogeneous volumes with multiple scattering
// with MIS between next event estimation and phase function sampling
// with surface lighting
Spectrum vol_path_tracing_5(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {
    // Homework 2: implememt this!
    return make_zero_spectrum();
}

// The final volumetric renderer: 
// multiple chromatic heterogeneous volumes with multiple scattering
// with MIS between next event estimation and phase function sampling
// with surface lighting
Spectrum vol_path_tracing(const Scene &scene,
                          int x, int y, /* pixel coordinates */
                          pcg32_state &rng) {
    // Homework 2: implememt this!
    return make_zero_spectrum();
}
