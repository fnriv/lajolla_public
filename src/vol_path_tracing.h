#pragma once

#include <optional>
#include <cmath>

// The simplest volumetric renderer: 
// single absorption only homogeneous volume
// only handle directly visible light sources
Spectrum vol_path_tracing_1(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {
    // Homework 2: implememt this!

    int w = scene.camera.width, h = scene.camera.height;
    Vector2 screen_pos((x + next_pcg32_real<Real>(rng)) / w,
                      (y + next_pcg32_real<Real>(rng)) / h);
    Ray ray = sample_primary(scene.camera, screen_pos);
    RayDifferential ray_diff = init_ray_differential(w, h);


    std::optional<PathVertex> isect = intersect(scene, ray, ray_diff);
    
    if (isect) {
        int medium_id = scene.camera.medium_id;

        Spectrum transmittance = make_const_spectrum(1);

        const Medium &medium = scene.media[medium_id];  // Get camera's medium
        Spectrum sigma_a = get_sigma_a(medium, isect->position);
        // std::cout << "sigma_a: " << sigma_a << std::endl;  // Should now print (1.5, 1.5, 1.5)
        
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
    // Homework 2: implememt this!

    /*
        pseudocode for this function:
        cameera_ray = sample_primary(camera, screen_pos, rng)
        isect = intersect(scene, camera_ray)

        u = next(rng) // u in [0, 1]
        t = -log(1- u) / sigma_t // sample a distance t using inversion sampling

        // calculate t_hit 

        if t < isect.t:
            // we have a scattering event before hitting the surface
            // calculate transmittance and in-scattering contribution
            
            trans_pdf = exp(-sigma_t * t) * sigma_t
            transmittance = exp(-sigma_a * t)

            // compute L_s1 using Monte Carlo sampling
            p = camera_ray.org + t * camera_ray.dir
            
            // Equation 7 from the writeup --> Lscatter1(p, dir_view)
            L_s1_estimate, L_s1_pdf = L_scatter1(p, sample_pt_on_light(rng))
            return (transmittance / trans_pdf) * sigma_s * (L_s1_estimate / L_s1_pdf)
            // return the product of them as the final radiance

        else:
            // we hit the surface before any scattering event happens
            // calculate transmittance and surface emission, and return their product as the final radiance
            trans_pdf = exp(-sigma_t * isect.t_hit)
            transmittance = exp(-sigma_t * isect.t_hit)
            Le = 0
            if is_light(isect):
                Le = isect.Le
            return (transmittance / trans_pdf) * Le
    */

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
    
    // sigma_t = sigma_a + sigma_s
    Spectrum sigma_t = sigma_a + sigma_s;

    Real u = next_pcg32_real<Real>(rng);
    Real t = -log(1 - u) / sigma_t.x; // Assuming monochromatic, we can use any channel of sigma_t

    
    int medium_id = scene.camera.medium_id;

    if (medium_id < 0) {
        // No medium, just return emission
        if (is_light(scene.shapes[isect->shape_id])) {
            return emission(*isect, -ray.dir, scene);
        }
        return make_zero_spectrum();
    }

    if (t < t_hit) {
        // std::cout << "Scattering event at t = " << t << ", before hitting surface at t_hit = " << t_hit << std::endl;
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

// The third volumetric renderer (not so simple anymore): 
// multiple monochromatic homogeneous volumes with multiple scattering
// no need to handle surface lighting, only directly visible light source
Spectrum vol_path_tracing_3(const Scene &scene,
                            int x, int y, /* pixel coordinates */
                            pcg32_state &rng) {
    // Homework 2: implememt this!
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
    return make_zero_spectrum();
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
