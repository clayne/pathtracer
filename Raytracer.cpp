// Raytracer.cpp : Defines the entry point for the console application.
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <string>
#include "chrono.h"
#include "Raytracer.h"

#undef max
#undef min


inline double sqr(double x) { return x*x; }


double int_exponential(double y0, double ysol, double beta, double s, double uy) {
	double 	result = 0.1 * exp((-y0 + ysol)*beta) * (1 - exp(-s*uy*beta)) / (uy*beta);
	return result;
}

Vector Raytracer::getColor(const Ray &r, const Scene &s, int nbrebonds, int screenI, int screenJ, bool show_lights, bool no_envmap) {

	if (nbrebonds == 0) return Vector(0, 0, 0);

	Vector centerLight = s.lumiere->O + s.lumiere->get_translation(r.time, is_recording);

	Vector P;
	MaterialValues mat;
	int sphere_id, tri_id;
	double t;
	bool has_inter = s.intersection(r, P, sphere_id, t, mat, tri_id);
	Vector N = mat.shadingN;	

	/*Vector randV(uniform(engine), uniform(engine), uniform(engine));
	Vector T1 = cross(randV, N); T1.normalize();
	Vector T2 = cross(T1, N);*/

//	Vector newOrigin = P+0.1*N;// +-log(uniform(engine))*0.15*T1 + -log(uniform(engine))*0.15*T2 + 0.1*N;   // BEWARE: not used for normal maps!

	// we just replace the envmap by the background image for direct rays
	if ( (nbrebonds == nb_bounces) && (s.backgroundW > 0) && (s.background.size() == s.backgroundW*s.backgroundH * 3) && (!has_inter || (has_inter && sphere_id==1 && dynamic_cast<Sphere*>(s.objects[1]) ))) {
		int i = std::min(s.backgroundH - 1, std::max(0, (int)(screenI / (double)H*s.backgroundH)));
		int j = std::min(s.backgroundW - 1, std::max(0, (int)(screenJ / (double)W*s.backgroundW)));
		double r = s.background[i*s.backgroundW * 3 + j * 3];
		double g = s.background[i*s.backgroundW * 3 + j * 3 + 1];
		double b = s.background[i*s.backgroundW * 3 + j * 3 + 2];
		return Vector(r, g, b);
	}
	double lum_scale = s.lumiere->get_scale(r.time, is_recording);

	Vector intensite_pixel(0, 0, 0);
	if (has_inter) {
		int threadid = omp_get_thread_num();

		if (sphere_id == 1) {
			if (no_envmap)
				return Vector(0, 0, 0);
			else {
				Sphere* env = dynamic_cast<Sphere*>(s.objects[1]);
				if (env) {
					return s.envmap_intensity*mat.Ke;
				}
			}				
		}
		if (sphere_id == 0) {
			intensite_pixel = show_lights ? (/*s.lumiere->albedo **/Vector(1.,1.,1.)* (s.intensite_lumiere / sqr(lum_scale))) : Vector(0., 0., 0.);
		} else {
			BRDF* brdf = s.objects[sphere_id]->brdf->clone();  // thread safety issues if not cloned
			brdf->setParameters(mat);

			intensite_pixel += mat.Ke*s.envmap_intensity;

			if (s.objects[sphere_id]->miroir) {
				Vector direction_miroir = r.direction.reflect(N);
				Ray rayon_miroir(P + 0.001*N, direction_miroir, r.time);

				//if (uniform(engine) < 0.9)
				intensite_pixel += getColor(rayon_miroir, s, nbrebonds - 1, screenI, screenJ, show_lights, no_envmap);// / 0.9;

			} else
				if (mat.transp) {
					double n1 = 1;
					double n2 = mat.refr_index;
					Vector normale_pour_transparence(N);
					Ray new_ray;
					bool entering = true;
					if (dot(r.direction, N) > 0) {  // on sort de la sphere
						n1 = mat.refr_index;
						n2 = 1;
						normale_pour_transparence = -N;
						entering = false;
					}
					double radical = 1 - sqr(n1 / n2)*(1 - sqr(dot(normale_pour_transparence, r.direction)));
					if (radical > 0) {
						Vector direction_refracte = (n1 / n2)*(r.direction - dot(r.direction, normale_pour_transparence)*normale_pour_transparence) - normale_pour_transparence * sqrt(radical);
						Ray rayon_refracte(P - 0.001*normale_pour_transparence, direction_refracte, r.time);

						double R0 = sqr((n1 - n2) / (n1 + n2));
						double R;
						if (entering) {
							R = R0 + (1 - R0)*std::pow(1 + dot(r.direction, N), 5);
						} else {
							R = R0 + (1 - R0)*std::pow(1 - dot(direction_refracte, N), 5);
						}

						if (uniform(engine[threadid]) < R) {
							new_ray = Ray(P + 0.001*normale_pour_transparence, r.direction.reflect(N), r.time);
						} else {
							new_ray = Ray(P - 0.001*normale_pour_transparence, direction_refracte, r.time);
						}
					} else {
						new_ray = Ray(P + 0.001*normale_pour_transparence, r.direction.reflect(N), r.time);
						//return Vector(0, 0, 0);
					}
					intensite_pixel += getColor(new_ray, s, nbrebonds - 1, screenI, screenJ, show_lights, no_envmap);
				} else {

					//if (dot(N, r.direction) > 0) N = -N;

			// contribution de l'eclairage direct
				/*Ray ray_light(P + 0.01*N, (s.lumiere->O - P).getNormalized(), r.time);
				Vector P_light, N_light, color_light;
				int sphere_id_light;
				double t_light;
				bool has_inter_light = s.intersection(ray_light, P_light, N_light, sphere_id_light, t_light, color_light);
				double d_light2 = (s.lumiere->O - P).getNorm2();

				if (has_inter_light && t_light*t_light < d_light2) {
					intensite_pixel = Vector(0, 0, 0);
				}
				else {
					intensite_pixel = albedo / M_PI * s.intensite_lumiere * std::max(0., dot((s.lumiere->O - P).getNormalized(), N)) / d_light2;
				}*/
	
					Vector axeOP = (P - centerLight).getNormalized();
					Vector dir_aleatoire = random_cos(axeOP);
					Vector point_aleatoire = dir_aleatoire * s.lumiere->R*lum_scale + centerLight;
					Vector wi = (point_aleatoire - P).getNormalized();
					double d_light2 = (point_aleatoire - P).getNorm2();
					Vector Np = dir_aleatoire;
					//Vector kd =  mat.Kd;
					Vector tr;
					if (s.objects[sphere_id]->ghost) {
						Vector offset;   // try to be robust here since we don't do nbrebonds-1
						if (dot(N, r.direction) > 0)
							offset = N;
						else
							offset = -N;
						tr = getColor(Ray(P + r.direction*0.001 + offset*0.001, r.direction, r.time), s, nbrebonds, screenI, screenJ, show_lights, no_envmap);
					}
					Ray ray_light(P + 0.01*wi, wi, r.time);
					double t_light;
					bool has_inter_light = s.intersection_shadow(ray_light, t_light, sqrt(d_light2), true);
					bool direct_visible = true;
					if ((has_inter_light) ) {
						intensite_pixel += Vector(0, 0, 0);
						direct_visible = false;
					} else {
						//intensite_pixel = (s.intensite_lumiere / (4.*M_PI*d_light2) * std::max(0., dot(N, wi)) * dot(Np, -wi) / dot(axeOP, dir_aleatoire))*(M_PI) * ((1. - s.objects[sphere_id]->ks)*albedo/ (M_PI) + Phong_BRDF(wi, r.direction, N, s.objects[sphere_id]->phong_exponent)*s.objects[sphere_id]->ks * albedo);
						if (dot(N, wi) < 0) N = -N;
						//Vector BRDF = albedo / M_PI   * (1. - s.objects[sphere_id]->ks)   + s.objects[sphere_id]->ks*Phong_BRDF(wi, r.direction, N, s.objects[sphere_id]->phong_exponent)*albedo;
						Vector BRDF = brdf->eval(wi, -r.direction, N);// kd / M_PI + Phong_BRDF(wi, r.direction, N, mat.Ne)*mat.Ks;
						double J = 1.*dot(Np, -wi) / d_light2;
						double proba = dot(axeOP, dir_aleatoire) / (M_PI * s.lumiere->R*s.lumiere->R*lum_scale*lum_scale);
						if (s.objects[sphere_id]->ghost) {
							intensite_pixel += tr;
						} else {
							if (proba > 0) {
								intensite_pixel += (s.intensite_lumiere / sqr(lum_scale) * std::max(0., dot(N, wi)) * J / proba)  * BRDF;
							}
						}
						
							
					}

					// ajout de la contribution indirecte
					double proba_globale;
					Vector direction_aleatoire = brdf->sample(-r.direction, N, proba_globale);

					if (dot(direction_aleatoire, N) < 0 || dot(direction_aleatoire, r.direction.reflect(N)) < 0 || proba_globale <= 0) {
						delete brdf;
						return intensite_pixel;
					}


					Ray	rayon_aleatoire(P + 0.01*direction_aleatoire, direction_aleatoire, r.time);

					intensite_pixel += getColor(rayon_aleatoire, s, nbrebonds - 1, screenI, screenJ, false, no_envmap)  * ((dot(N, direction_aleatoire) / proba_globale)) *brdf->eval(direction_aleatoire, -r.direction, N);

					/*if (s.objects[sphere_id]->ghost) {
						//if (nbrebonds != nb_bounces)
						//	return intensite_pixel;
						if (sample_diffuse)
							intensite_pixel += tr / 196964.699 / M_PI *getColor(rayon_aleatoire, s, nbrebonds - 1, screenI, screenJ, false, direct_visible)  / proba_globale;
						else
							intensite_pixel += getColor(rayon_aleatoire, s, nbrebonds - 1, screenI, screenJ, false, true)  * Phong_BRDF(direction_aleatoire, r.direction, N, mat.Ne) / proba_globale *mat.Ks;
					} else {
						if (sample_diffuse)
							intensite_pixel += getColor(rayon_aleatoire, s, nbrebonds - 1, screenI, screenJ, false, no_envmap) * ((dot(N, direction_aleatoire) / (M_PI) / proba_globale)) * kd;
						else
							intensite_pixel += getColor(rayon_aleatoire, s, nbrebonds - 1, screenI, screenJ, false, no_envmap)  * ((dot(N, direction_aleatoire) / proba_globale)) *mat.Ks * Phong_BRDF(direction_aleatoire, r.direction, N, mat.Ne);
					}*/
					//intensite_pixel += getColor(rayon_aleatoire, s, nbrebonds - 1, false)  * dot(N, direction_aleatoire) * Phong_BRDF(direction_aleatoire, r.direction, N, s.objects[sphere_id]->phong_exponent)*s.objects[sphere_id]->ks * albedo / proba_globale;
				}
				delete brdf;
		}
	}
	if (s.fog_density==0)
		return intensite_pixel;

	Vector Lv(0., 0., 0.);
	
	double p_uniform = 0.5;
	bool is_uniform_fog = (s.fog_type==0);
	double beta = s.fog_density;// is_uniform_fog ? 0.04 : 0.1;
	bool uniform_sampling_ray = true;
	int phase = 0; // 0 : uniform, 1: Schlick, 2: Rayleigh

	double int_ext;
	if (is_uniform_fog) {
		int_ext = beta * t;
	} else {
		int_ext = int_exponential(r.origin[1], s.objects[2]->get_translation(r.time, is_recording)[1], beta, t, r.direction[1]);
	}
	double T = exp(-int_ext);

	int threadid = omp_get_thread_num();
	double proba_t, random_t;
	double clamped_t = std::min(1000., t);
	if (uniform_sampling_ray) {		
		random_t = uniform(engine[threadid])*clamped_t;
		proba_t = 1. / clamped_t;
	} else {
		double alpha = 5. / clamped_t;

		do {
			random_t = -log(uniform(engine[threadid])) / alpha;
		} while (random_t > clamped_t);

		double normalization = 1. / alpha * (1 - exp(-alpha*clamped_t));
		proba_t = exp(-alpha*random_t) / normalization;
	}

	double int_ext_partielle;
	if (is_uniform_fog) {
		int_ext_partielle = beta * random_t;
	} else {
		int_ext_partielle = int_exponential(r.origin[1], s.objects[2]->get_translation(r.time, is_recording)[1], beta, random_t, r.direction[1]);
	}
		

	Vector random_P = r.origin + random_t*r.direction;

	Vector random_dir;
	double proba_dir;

	Vector point_aleatoire;
	Vector axeOP = (random_P - centerLight).getNormalized();
	bool is_uniform;
	if (uniform(engine[threadid]) < p_uniform) {
		random_dir = random_uniform();				
		is_uniform = true;
	} else {		
		Vector dir_aleatoire = random_cos(axeOP);
		point_aleatoire = dir_aleatoire * s.lumiere->R*lum_scale + centerLight;
		random_dir = (point_aleatoire - random_P).getNormalized();
		is_uniform = false;
	}


	double phase_func;
	double k = 0.4;
	switch (phase) {
	case 0:
		phase_func = 0.3 / (4.*M_PI);
		break;
	case 1:		
		phase_func = (1 - k*k) / (4.*M_PI*(1 + k*dot(random_dir, -r.direction)));
		break;
	case 2:
		phase_func = 3 / (16 * M_PI)*(1 + sqr(dot(random_dir, r.direction)));
		break;
	}

	Ray L_ray(random_P, random_dir, r.time);
	MaterialValues interMat;
	Vector interP;
	int interid, intertri;
	double intert;
	bool interinter = s.intersection(L_ray, interP, interid, intert, interMat, intertri);
	const Vector& interN = interMat.shadingN;

	double V;
	if (is_uniform) {
		V = 1;
	} else {
		double d_light2 = (point_aleatoire - random_P).getNorm2();
		if (interinter && intert*intert < d_light2*0.99) {
			V = 0;
		} else {
			V = 1;
		}
	}

	if (V == 0) {
		Lv = Vector(0., 0., 0.);
	} else {
		double pdf_uniform = 1. / (4.*M_PI);
		double J = dot(interN, -random_dir) / (interP - random_P).getNorm2();
		double pdf_light = (interinter && interid==0) ? (dot((interP - centerLight).getNormalized(), axeOP) / (M_PI * sqr(s.lumiere->R*lum_scale)) / J) : 0.;
		proba_dir = p_uniform * pdf_uniform + (1 - p_uniform)*pdf_light;

		Vector L = getColor(L_ray, s, nbrebonds - 1, screenI, screenJ);

		double ext;
		if (is_uniform_fog) {
			ext = beta;
		} else {
			ext = 0.1 * exp(-beta*(random_P[1] - s.objects[2]->get_translation(r.time, is_recording)[1]));
		}
		Lv = L * phase_func * ext * exp(-int_ext_partielle) / (proba_t * proba_dir);
	}

	return intensite_pixel * T + Lv;
}

void Raytracer::save_scene(const char* filename) {
	
	FILE* f = fopen(filename, "w+");
	fprintf(f, "W,H: %u, %u\n", W, H);
	fprintf(f, "nrays: %u\n", nrays);
	fprintf(f, "nbframes: %u\n", s.nbframes);
	fprintf(f, "Cam: (%lf, %lf, %lf), (%lf, %lf, %lf), (%lf, %lf, %lf)\n", cam.position[0], cam.position[1], cam.position[2], cam.direction[0], cam.direction[1], cam.direction[2], cam.up[0], cam.up[1], cam.up[2]);
	fprintf(f, "fov: %lf\n", cam.fov);
	fprintf(f, "focus: %lf\n", cam.focus_distance);
	fprintf(f, "aperture: %lf\n", cam.aperture);
	fprintf(f, "sigma_filter: %lf\n", sigma_filter);
	fprintf(f, "gamma: %lf\n", gamma);
	fprintf(f, "bounces: %u\n", nb_bounces);
	fprintf(f, "intensite_lum: %le\n", s.intensite_lumiere);
	fprintf(f, "intensite_envmap: %lf\n", s.envmap_intensity);
	if (s.backgroundfilename.size()>0)
		fprintf(f, "background: %s\n", s.backgroundfilename.c_str());
	

	fprintf(f, "nbobjects: %u\n", static_cast<unsigned int>(s.objects.size()));
	for (int i = 0; i < s.objects.size(); i++) {
		s.objects[i]->save_to_file(f);
	}

	fprintf(f, "fog_density: %lf\n", s.fog_density);
	fprintf(f, "fog_type: %u\n", s.fog_type);	
	fclose(f);
}


void Raytracer::load_scene(const char* filename, const char* replacedNames) {

	s.clear();
	char line[255], bg[255];

	FILE* f = fopen(filename, "r");
	fscanf(f, "W,H: %u, %u\n", &W, &H);
	fscanf(f, "nrays: %u\n", &nrays);
	fscanf(f, "%[^\n]\n", line);
	if (line[0] == 'n') {
		sscanf(line, "nbframes: %u\n", &s.nbframes);
		fscanf(f, "Cam: (%lf, %lf, %lf), (%lf, %lf, %lf), (%lf, %lf, %lf)\n", &cam.position[0], &cam.position[1], &cam.position[2], &cam.direction[0], &cam.direction[1], &cam.direction[2], &cam.up[0], &cam.up[1], &cam.up[2]);
	} else {
		sscanf(line, "Cam: (%lf, %lf, %lf), (%lf, %lf, %lf), (%lf, %lf, %lf)\n", &cam.position[0], &cam.position[1], &cam.position[2], &cam.direction[0], &cam.direction[1], &cam.direction[2], &cam.up[0], &cam.up[1], &cam.up[2]);
	}
	fscanf(f, "fov: %lf\n", &cam.fov);
	fscanf(f, "focus: %lf\n", &cam.focus_distance);
	fscanf(f, "aperture: %lf\n", &cam.aperture);
	fscanf(f, "sigma_filter: %lf\n", &sigma_filter);
	
	int nbo;
	fscanf(f, "%[^\n]\n", line);
	if (line[0] == 'g') {
		sscanf(line, "gamma: %lf\n", &gamma);
		fscanf(f, "bounces: %u\n", &nb_bounces);
	} else {
		sscanf(line, "bounces: %u\n", &nb_bounces);
	}
	fscanf(f, "intensite_lum: %le\n", &s.intensite_lumiere);
	fscanf(f, "intensite_envmap: %lf\n", &s.envmap_intensity);


	fscanf(f, "%[^\n]\n", line);
	if (line[0] == 'n') {
		sscanf(line, "nbobjects: %u\n", &nbo);
		s.clear_background();
	} else {
		sscanf(line, "background: %[^\n]\n", bg);
		s.load_background(bg, gamma);
		fscanf(f, "nbobjects: %u\n", &nbo);
	}

	for (int i = 0; i < nbo; i++) {
		s.objects.push_back(Object::create_from_file(f, replacedNames));
	}

	fscanf(f, "fog_density: %lf\n", &s.fog_density);
	fscanf(f, "fog_type: %u\n", &s.fog_type);

	fclose(f);

	s.lumiere = dynamic_cast<Sphere*>(s.objects[0]);
}

void Raytracer::loadScene() {

	//W = 1000;
	//H = 1200;
#ifdef _DEBUG
	W = 320;
	H = 400;
#else
	W = 1000;
	H = 800;
#endif
	nrays = 1000;   //10 pour debugguer, 1000 pour les rendus finaux	
	cam = Camera(Vector(0., 0., 50.), Vector(0, 0, -1), Vector(0, 1, 0));
	cam.fov = 35 * M_PI / 180;   // field of view
	cam.focus_distance = 50; // distance mise au point	
	cam.aperture = 0.1;     // ouverture, pour depth of field (faible valeur = net partout)
	sigma_filter = 0.5;  // antialiasing
	nb_bounces = 3;

	Sphere* slum = new Sphere(Vector(10, 23, 15), 10);   //sphere light: position, rayon, albedo (qui sert � rien)
	Sphere* s2 = new Sphere(Vector(0, 0, 0), 1000000); s2->flip_normals = true; // envmap  

	Plane* plane = new Plane(Vector(0, 0, 0), Vector(0., 1., 0.));
	plane->max_translation = Vector(0.,-27.3,0.);

	
	s.addObject(slum);   // sphere light
	s.addObject(s2); // envmap
	s.addObject(plane);
	

	s.lumiere = slum;
	s.intensite_lumiere = 1000000000 * 4.*M_PI / (4.*M_PI*s.lumiere->R*s.lumiere->R*M_PI);
	s.envmap_intensity = 1;

	cam.rotate(0, -22 * M_PI / 180, 1); // helmet
}

double sum_area_table(double* sat, int sat_width, int i0, int i1, int j0, int j1) {

	double term1 = 0;
	if (i0 > 0) {
		term1 = sat[(i0 - 1)*sat_width + j1];
	}
	double term2 = 0;
	if (j0 > 0) {
		term2 = sat[i1*sat_width + j0-1];
	}
	double term3 = 0;
	if (i0>0 && j0 > 0) {
		term3 = sat[(i0-1)*sat_width + j0 - 1];
	}
	return sat[i1*sat_width + j1] - term1 - term2 + term3;
}

//A Fast, Compact Approximation of the Exponential Function
double fast_exp(double y) {
 double d;
* ((int*)(&d) + 0) = 0;
* ((int*)(&d) + 1) = (int)(1512775 * y + 1072632447);
return d;
}
void Raytracer::render_image()
{

	int fstart = 0;
	int fend = 1;
	Wlr = ceil(W / 16.);
	Hlr = ceil(H / 16. );
	computed.resize(W*H, false);
	imagedouble.resize(W*H * 3, 0);  
	imagedouble_lowres.resize(Wlr*Hlr * 3, 0);
	sample_count.resize(W*H, 0);
	
	std::fill(computed.begin(), computed.end(), false);
	std::fill(sample_count.begin(), sample_count.end(), 0);
	std::fill(imagedouble_lowres.begin(), imagedouble_lowres.end(), 0);

	for (int i = 0; i < omp_get_max_threads(); i++) {
		engine[i] = std::default_random_engine(i);
	}
	
	//static int nbcalls = 0;
	std::vector<std::pair<int, int> > permut(64);
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			permut[i * 8 + j] = std::make_pair(i, j);
	std::random_shuffle(permut.begin(), permut.end());

	int filter_size = std::ceil(sigma_filter * 2);
	int filter_total_width = 2 * filter_size + 1;
	filter_integral.resize(filter_total_width*filter_total_width);	
	filter_value.resize(filter_total_width*filter_total_width);
	for (int i = -filter_size; i <= filter_size; i++) {
		for (int j = -filter_size; j <= filter_size; j++) {
			double integ = 0;
			for (int i2 = -filter_size; i2 <= i; i2++) {
				for (int j2 = -filter_size; j2 <= j; j2++) {
					double w = fast_exp(-(i2*i2+j2*j2) / (2.*sigma_filter*sigma_filter)) / (sigma_filter*sigma_filter*2.*M_PI);
					integ += w;
				}
			}
			filter_integral[(i + filter_size)*filter_total_width + (j + filter_size)] = integ;
			filter_value[(i + filter_size)*filter_total_width + (j + filter_size)] = exp(-(i*i + j*j) / (2.*sigma_filter*sigma_filter)) / (sigma_filter*sigma_filter*2.*M_PI);
		}
	}


	/*double test = sum_area_table(&filter_integral[0], filter_total_width, filter_size- filter_size, filter_size+15, filter_size+8, filter_size+11);
	double integ = 0;
	for (int i2 = -filter_size; i2 <= 15; i2++) {
		for (int j2 = 8; j2 <= 11; j2++) {
			double w = exp(-(i2*i2 + j2*j2) / (2.*sigma_filter*sigma_filter)) / (sigma_filter*sigma_filter*2.*M_PI);
			integ += w;
		}
	}*/



	for (int time_step = fstart; time_step < fend; time_step++) {

		for (int i = 0; i < s.objects.size(); i++) {
			s.objects[i]->build_matrix(s.current_frame, is_recording); // time = 0 here
		}

		for (int k = 0; k < nrays; k++) {
			current_nb_rays = k;
			chrono.Start();
			for(int pix_in_block=0; pix_in_block<64; pix_in_block++) {
				int i1 = permut[pix_in_block].first;
				int j1 = permut[pix_in_block].second;
			//for (int i1 = 0; i1 < 8; i1++) {
				//for (int j1 = 0; j1 < 8; j1++) {
					if (stopped) {						
						return;
					}
#pragma omp parallel for schedule(dynamic, 2)
					for (int i = i1; i < H; i += 8) {
						int threadid = omp_get_thread_num();

						for (int j = j1; j < W; j += 8) {

							double dx = uniform(engine[threadid]) - 0.5;
							double dy = uniform(engine[threadid]) - 0.5;

							double dx_aperture = (uniform(engine[threadid]) - 0.5) * cam.aperture;
							double dy_aperture = (uniform(engine[threadid]) - 0.5) * cam.aperture;

							double time = s.current_time + time_step / 60. + uniform(engine[threadid]) / 60.;



							Ray r = cam.generateDirection(i, j, time, dx, dy, dx_aperture, dy_aperture, W, H);

							Vector color = getColor(r, s, nb_bounces, i, j);

							int bmin_i = std::max(0, i - filter_size);
							int bmax_i = std::min(i + filter_size, H - 1);
							int bmin_j = std::max(0, j - filter_size);
							int bmax_j = std::min(j + filter_size, W - 1);
							double ratio = 1. / sum_area_table(&filter_integral[0], filter_total_width, bmin_i - i + filter_size, bmax_i - i + filter_size, bmin_j - j + filter_size, bmax_j - j + filter_size);
							double denom1 = ratio / (sigma_filter*sigma_filter*2.*M_PI);
							double denom2 = 1. / (2.*sigma_filter*sigma_filter);
							for (int i2 = bmin_i; i2 <= bmax_i; i2++) {
								for (int j2 = bmin_j; j2 <= bmax_j; j2++) {
									//double w = filter_value[(i2 - i + filter_size)*filter_total_width + j2-j + filter_size] * ratio; 

									double w = fast_exp(-(sqr(i2-i-dy)+ sqr(j2 - j - dx)) *denom2) *denom1;
									imagedouble[((H - i2 - 1)*W + j2) * 3 + 0] += color[0] * w;
									imagedouble[((H - i2 - 1)*W + j2) * 3 + 1] += color[1] * w;
									imagedouble[((H - i2 - 1)*W + j2) * 3 + 2] += color[2] * w;																		
									//sw += w;
									sample_count[(H - i2 - 1)*W + j2] += w; // 1. / sqr(filter_total_width);
								}
							}
							//sample_count[(H - i - 1)*W + j] += sw;

							computed[(H - i - 1)*W + j] = true;

							imagedouble_lowres[((Hlr - (i)/16 - 1)*Wlr + (j)/16) * 3 + 0] += color[0]/256.;
							imagedouble_lowres[((Hlr - (i)/16 - 1)*Wlr + (j)/16) * 3 + 1] += color[1] / 256.;
							imagedouble_lowres[((Hlr - (i)/16 - 1)*Wlr + (j)/16) * 3 + 2] += color[2] / 256.;

							/*bmin_i = std::max(0, i/16 - filter_size);
							bmax_i = std::min(i/16 + filter_size, Hlr - 1);
							bmin_j = std::max(0, j/16 - filter_size);
							bmax_j = std::min(j/16 + filter_size, Wlr - 1);
							ratio = 1. / sum_area_table(&filter_integral[0], filter_total_width, bmin_i - i/16 + filter_size, bmax_i - i/16 + filter_size, bmin_j - j/16 + filter_size, bmax_j - j/16 + filter_size);
							double sw = 0;
							for (int i2 = bmin_i; i2 <= bmax_i; i2++) {
								for (int j2 = bmin_j; j2 <= bmax_j; j2++) {
									double w = filter_value[(i2 - i/16 + filter_size)*filter_total_width + j2 - j/16 + filter_size] * ratio/256.;
									imagedouble_lowres[((Hlr - i2 - 1)*Wlr + j2) * 3 + 0] += color[0] * w;
									imagedouble_lowres[((Hlr - i2 - 1)*Wlr + j2) * 3 + 1] += color[1] * w;
									imagedouble_lowres[((Hlr - i2 - 1)*Wlr + j2) * 3 + 2] += color[2] * w;
									sw += w;
								}
							}*/

						}
					}
				}
			//}

			curTimePerFrame = chrono.GetDiffMs();
			/*nbcalls++;
			if (nbcalls%12==0)
				std::cout << nbcalls / (double)H * 100. << std::endl;*/
		}


		image.resize(W*H * 3, 0);
#pragma omp parallel for 
		for (int i = 0; i < H; i++) {
			for (int j = 0; j < W; j++) {
				image[((H - i - 1)*W + j) * 3 + 0] = std::min(255., std::max(0., 255.*std::pow(imagedouble[((H - i - 1)*W + j) * 3 + 0] / 196964.7 / (nrays + 1), 1 / gamma)));   // rouge
				image[((H - i - 1)*W + j) * 3 + 1] = std::min(255., std::max(0., 255.*std::pow(imagedouble[((H - i - 1)*W + j) * 3 + 1] / 196964.7 / (nrays + 1), 1 / gamma))); // vert
				image[((H - i - 1)*W + j) * 3 + 2] = std::min(255., std::max(0., 255.*std::pow(imagedouble[((H - i - 1)*W + j) * 3 + 2] / 196964.7 / (nrays + 1), 1 / gamma))); // bleu
			}
		}

		std::ostringstream os;
		//os << "testFog_" << time_step << ".bmp";
		os << "exportD" << s.current_frame << ".bmp";
		save_image(os.str().c_str(), &image[0], W, H);
	}

	//std::cout << chrono.GetDiffMs() / (double)1000. << std::endl;
    return;
}

