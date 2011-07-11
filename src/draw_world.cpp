// 3D World - OpenGL CS184 Computer Graphics Project
// by Frank Gennari
// 3/10/02
#include "3DWorld.h"
#include "mesh.h"
#include "lightmap.h"
#include "transform_obj.h"
#include "player_state.h"
#include "tree_leaf.h"
#include "textures_3dw.h"
#include "dynamic_particle.h"
#include "physics_objects.h"
#include "explosion.h"
#include "gl_ext_arb.h"


bool const DEBUG_COLORCODE      = 0;
bool const DEBUG_COLOR_COLLS    = 0;
bool const DYNAMIC_OBJ_LIGHTS   = 1;
bool const SHOW_DRAW_TIME       = 0;
bool const NO_SHRAP_DLIGHT      = 1; // looks cool with dynamic lights, but very slow
bool const DYNAMIC_SMOKE_SHADOWS= 1; // slower, but looks nice
unsigned const MAX_CFILTERS     = 10;
float const NDIV_SCALE          = 1.6;
float const CLOUD_WIND_SPEED    = 0.00015;


struct sky_pos_orient {

	point center;
	float radius, dx, dy;
	sky_pos_orient(point const &c, float r, float dx_, float dy_) : center(c), radius(r), dx(dx_), dy(dy_) {}
};


// Global Variables
bool invalid_ccache(1);
float sun_radius, moon_radius, earth_radius, brightness(1.0);
colorRGBA cur_ambient(0.0, 0.0, 0.0, 1.0), last_a(BLACK), last_d(BLACK);
point sun_pos, moon_pos;
point gl_light_positions[8] = {all_zeros};
point const earth_pos(-15.0, -8.0, 21.0);
sky_pos_orient cur_spo(point(0,0,0),0,0,0);
vector3d up_norm(plus_z);
vector<camera_filter> cfilters;
vector<light_source> enabled_lights;
pt_line_drawer obj_pld;
pt_line_drawer_hdr snow_pld;


extern GLUquadricObj* quadric;
extern bool have_sun, underwater, have_drawn_cobj, using_lightmap, has_dl_sources, has_dir_lights, smoke_exists;
extern int is_cloudy, do_zoom, xoff, yoff, xoff2, yoff2, iticks, display_mode, show_fog;
extern int num_groups, frame_counter, world_mode, island, teams, begin_motion, UNLIMITED_WEAPONS;
extern int window_width, window_height, game_mode, enable_fsource, draw_model, camera_mode, animate2;
extern unsigned smoke_tid, dl_tid, num_stars;
extern float zmin, light_factor, water_plane_z, fticks, perspective_fovy, perspective_nclip;
extern float temperature, atmosphere, TIMESTEP, base_gravity, tan_term, zbottom, sun_rot, leaf_size;
extern point light_pos, ocean, mesh_origin, flow_source, surface_pos, litning_pos, leaf_points[], star_pts[];
extern vector3d wind;
extern colorRGBA bkg_color, sun_color;
extern lightning l_strike;
extern vector<spark_t> sparks;
extern vector<star> stars;
extern obj_group obj_groups[];
extern vector<coll_obj> coll_objects;
extern obj_type object_types[];
extern obj_vector_t<bubble> bubbles;
extern obj_vector_t<particle_cloud> part_clouds, cloud_volumes;
extern obj_vector_t<fire> fires;
extern obj_vector_t<scorch_mark> scorches;
extern float diffuse[], gauss_rand_arr[];
extern cube_t cur_smoke_bb;
extern texture textures[];
extern player_state *sstates;
extern int coll_id[];
extern vector<light_source> dl_sources;
extern vector<portal> portals;



void draw_cloud_volumes();
void draw_sized_point(dwobject const &obj, float radius, float cd_scale, const colorRGBA &color, const colorRGBA &tcolor,
					  bool do_texture, bool is_shadowed, int is_chunky=0);
void draw_weapon2(dwobject const &obj, float radius);
void draw_ammo(obj_group &objg, float radius, const colorRGBA &color, int ndiv, int j, bool is_shadowed);
void draw_smiley_part(point const &pos, point const &pos0, vector3d const &orient, int type,
					  int use_orient, int ndiv, bool is_shadowed, float scale=1.0);
void draw_smiley(point const &pos, vector3d const &orient, float radius, int ndiv, int time,
				 float health, int id, bool is_shadowed, mesh2d const *const mesh);
void draw_powerup(point const &pos, float radius, int ndiv, int type, const colorRGBA &color, bool is_shadowed);
void draw_rolling_obj(point const &pos, point &lpos, float radius, int status, int ndiv, bool on_platform, int tid, xform_matrix *matrix);
void draw_skull(point const &pos, vector3d const &orient, float radius, int status, int ndiv);
void draw_rocket(point const &pos, vector3d const &orient, float radius, int type, int ndiv, int time, bool is_shadowed);
void draw_seekd(point const &pos, vector3d const &orient, float radius, int type, int ndiv, bool is_shadowed);
void draw_landmine(point pos, float radius, int ndiv, int time, int source, bool is_shadowed, bool in_ammo);
void draw_plasma(point const &pos, point const &part_pos, float radius, float size, int ndiv, int shpere_tex, bool gen_parts, int time);
void draw_chunk(point const &pos, float radius, vector3d const &v, vector3d const &vdeform, int charred, int ndiv, bool is_shadowed);
void draw_grenade(point const &pos, vector3d const &orient, float radius, int ndiv, int time, bool is_shadowed, bool in_ammo, bool is_cgrenade);
void draw_star(point const &pos, vector3d const &orient, vector3d const &init_dir, float radius, float angle, int rotate);
void draw_shell_casing(point const &pos, vector3d const &orient, vector3d const &init_dir, float radius,
					   float angle, float cd_scale, bool is_shadowed, unsigned char type);
void draw_rotated_triangle(point const &pos, vector3d const &o, float radius, float angle, float tscale);
void draw_shrapnel(dwobject const &obj, float radius, bool is_shadowed);
void draw_particle(dwobject const &obj, float radius);

int same_team(int source, int target); // gameplay



void set_fill_mode() {

	glPolygonMode(GL_FRONT_AND_BACK, ((draw_model == 0) ? GL_FILL : GL_LINE));
}


int get_universe_ambient_light() {

	return ((world_mode == WMODE_UNIVERSE) ? GL_LIGHT1 : GL_LIGHT3);
}


void set_colors_and_enable_light(int light, float const ambient[4], float const diffuse[4]) {

	glEnable(light);
	glLightfv(light, GL_AMBIENT, ambient);
	glLightfv(light, GL_DIFFUSE, diffuse);
}


void set_gl_light_pos(int light, point const &pos, float w) {

	assert(light >= GL_LIGHT0 && light <= GL_LIGHT7);
	float position[4];
	for (unsigned i = 0; i < 3; ++i) position[i] = pos[i];
	position[3] = w;
	glLightfv(light, GL_POSITION, position);
	gl_light_positions[light - GL_LIGHT0] = pos;
}


void draw_solid_object_groups() {

	draw_waypoints();
	draw_select_groups(1);
	if (display_mode & 0x0200) d_part_sys.draw();
}


void draw_transparent_object_groups() {

	draw_select_groups(0);
}


void draw_select_groups(int solid) {

	if (!begin_motion) return;

	for (int i = 0; i < num_groups; ++i) {
		obj_group &objg(obj_groups[i]);

		if (objg.enabled && objg.temperature_ok() && objg.end_id > 0) {
			if ((objg.large_radius() && !(object_types[objg.type].flags & SEMI_TRANSPARENT)) == solid) {
				invalid_ccache = 1;
				draw_group(objg);
			}
		}
	}
}


inline void set_ad_colors(colorRGBA const &a, colorRGBA const &d) {

	if (invalid_ccache || a != last_a) {
		set_color_a(a);
		last_a = a;
	}
	if (invalid_ccache || d != last_d) {
		set_color_d(d);
		last_d = d;
	}
	invalid_ccache = 0;
}


bool get_shadowed_color(colorRGBA &color_a, point const &pos, bool &is_shadowed, bool precip, bool no_dynamic) {

	if (DYNAMIC_OBJ_LIGHTS && (using_lightmap || (!no_dynamic && has_dl_sources)) && color_a != BLACK) { // somewhat slow
		float const val(get_indir_light(color_a, WHITE, (pos + vector3d(0.0, 0.0, 0.01)), no_dynamic, (is_shadowed || precip), NULL, NULL)); // get above mesh
		if (precip && val < 1.0) is_shadowed = 1; // if precip, imply shadow status from indirect light value
		if (val < 0.1 && is_shadowed) return 0;
	}
	return 1;
}


void set_shadowed_color(colorRGBA const &color, point const &pos, bool is_shadowed, bool precip, bool no_dynamic) {

	colorRGBA a(color);
	get_shadowed_color(a, pos, is_shadowed, precip, no_dynamic);
	set_ad_colors(a, (is_shadowed ? colorRGBA(0.0, 0.0, 0.0, color.alpha) : color));
}


inline void scale_color_uw(colorRGBA &color) {

	if (underwater) {
		color.red   *= 0.45;
		color.green *= 0.45;
		color.blue  *= 0.85;
	}
}


// Note: incorrect if there is both a sun and a moon
bool pt_is_shadowed(point const &pos, int light, int status, float radius, int cid, bool fast, bool use_mesh) {

	if (use_mesh) {
		int const xpos(get_ypos(pos.x)), ypos(get_ypos(pos.y));
		if (point_outside_mesh(xpos, ypos)) return 0;

		if ((pos.z - 1.5*radius) < mesh_height[ypos][xpos]) {
			//if (is_mesh_disabled(xpos, ypos)) return 0; // assuming not drawing the mesh means it's underneath a cobj
			return ((shadow_mask[light][ypos][xpos] & SHADOWED_ALL) != 0);
		}
		if (fast) return (is_shadowed_lightmap(pos)); // use the precomputed lightmap value
	}
	return (!is_visible_to_light_cobj(pos, light, radius, cid, 0));
}


inline void set_obj_specular(bool shadowed, unsigned flags, float specular_brightness) {

	if (flags & SPECULAR) {
		set_specular((shadowed ? 0.0 : specular_brightness), 50.0);
	}
	else if (flags & LOW_SPECULAR) {
		set_specular((shadowed ? 0.0 : 0.4*specular_brightness), 10.0);
	}
}


void check_drawing_flags(unsigned flags, int init_draw, bool shadowed) {

	if (init_draw) {
		set_obj_specular(shadowed, flags, 0.5*brightness);
		if (flags & BLEND) enable_blend();
	}
	else {
		if (flags & BLEND) disable_blend();
		if (flags & (SPECULAR | LOW_SPECULAR)) set_specular(0.0, 1.0);
	}
}


inline void set_color_by_status(int status, point const &pos, bool is_shadowed) {

	colorRGBA const colors[6] = {BLACK, RED, WHITE, YELLOW, BLUE, GRAY};
	assert(status >= 0 && status < 6);
	set_shadowed_color(colors[status], pos, is_shadowed, 0, 0);
}


inline void set_color_v2(const colorRGBA &color, point const &pos, int status, bool is_shadowed, bool precip) {

	if (DEBUG_COLORCODE) {
		set_color_by_status(status, pos, is_shadowed);
	}
	else {
		set_shadowed_color(color, pos, is_shadowed, precip, 0);
	}
}


inline bool is_droplet(int type) {

	return ((object_types[type].flags & OBJ_IS_DROP) || type == HAIL || type == CHARRED);
}


inline bool get_cull_face(int type, colorRGBA const &color) {

	return (color.alpha < 1.0 && type != ROCKET && type != STAR5);
}


int color_wrapper::gl_type       = GL_UNSIGNED_BYTE;
int color_wrapper_float::gl_type = GL_FLOAT;


template class pt_line_drawer_t<color_wrapper      >;
template class pt_line_drawer_t<color_wrapper_float>;


template<typename cwt> void pt_line_drawer_t<cwt>::add_textured_pt(point const &v, colorRGBA c, int tid) {

	if (tid >= 0) c = c.modulate_with(texture_color(tid));
	vector3d const view_dir(get_camera_pos(), v);
	add_pt(v, view_dir, c);
}


template<typename cwt> void pt_line_drawer_t<cwt>::add_textured_line(point const &v1, point const &v2, colorRGBA c, int tid) {

	if (tid >= 0) c = c.modulate_with(texture_color(tid));
	vector3d view_dir(get_camera_pos(), (v1 + v2)*0.5);
	orthogonalize_dir(view_dir, (v2 - v1), view_dir, 0);
	add_line(v1, view_dir, c, v2, view_dir, c);
}


template<typename cwt> void pt_line_drawer_t<cwt>::vnc_cont::draw(int type) const {
	
	if (empty()) return; // nothing to do
	glVertexPointer(3, GL_FLOAT,     sizeof(vnc), &(front().v));
	glNormalPointer(   GL_FLOAT,     sizeof(vnc), &(front().n));
	glColorPointer( 4, cwt::gl_type, sizeof(vnc), &(front().c));
	glDrawArrays(type, 0, size());
}


template<typename cwt> void pt_line_drawer_t<cwt>::draw() const {
		
	if (points.empty() && lines.empty()) return;
	GLboolean const col_mat_en(glIsEnabled(GL_COLOR_MATERIAL));
	assert(!(lines.size() & 1));
	assert((triangles.size() % 3) == 0);
	if (!col_mat_en) glEnable(GL_COLOR_MATERIAL);
	set_array_client_state(1, 0, 1, 1);
	points.draw(GL_POINTS);
	lines.draw(GL_LINES);
	triangles.draw(GL_TRIANGLES);
	if (!col_mat_en) glDisable(GL_COLOR_MATERIAL);
	last_a = last_d = BLACK;
	//cout << "mem: " << get_mem() << endl;
}


void quad_batch_draw::add_quad_vect(vector<vert_norm> const &points, colorRGBA const &color) {
	
	assert(!(points.size() & 3)); // must be a multiple of 4
	float const tcx[4] = {0,1,1,0}, tcy[4] = {0,0,1,1}; // 00 10 11 01
	color_wrapper cw;
	cw.set_c3(color);

	for (unsigned i = 0; i < points.size(); ++i) {
		verts.push_back(vert_norm_tc_color(points[i].v, points[i].n, tcx[i&3], tcy[i&3], cw.c));
	}
	unsigned const batch_size(4096);
	if (size() > batch_size) draw_and_clear();
}


void quad_batch_draw::draw() const {
	
	if (verts.empty()) return;
	assert(!(verts.size() & 3)); // must be a multiple of 4
	verts.front().set_state();
	glDrawArrays(GL_QUADS, 0, size());
}


void vert_norm_tc_color::set_vbo_arrays(unsigned stride_mult) {

	assert(stride_mult > 0);
	set_array_client_state(1, 1, 1, 1);
	unsigned const stride(stride_mult*sizeof(vert_norm_tc_color));
	glVertexPointer  (3, GL_FLOAT,         stride, (void *)(0));
	glNormalPointer  (   GL_FLOAT,         stride, (void *)(sizeof(point)));
	glTexCoordPointer(2, GL_FLOAT,         stride, (void *)(sizeof(point) + sizeof(vector3d)));
	glColorPointer   (3, GL_UNSIGNED_BYTE, stride, (void *)(sizeof(point) + sizeof(vector3d) + 2*sizeof(float)));
}


void vert_norm_tc_color::set_state(unsigned stride_mult) const {
	
	assert(stride_mult > 0);
	set_array_client_state(1, 1, 1, 1);
	unsigned const stride(stride_mult*sizeof(*this));
	glVertexPointer  (3, GL_FLOAT,         stride, &v);
	glNormalPointer  (   GL_FLOAT,         stride, &n);
	glTexCoordPointer(2, GL_FLOAT,         stride, &t);
	glColorPointer   (3, GL_UNSIGNED_BYTE, stride, &c);
}


void vert_color::set_state(unsigned vbo) const { // typically called on element 0
	
	unsigned const stride(sizeof(*this));
	set_array_client_state(1, 0, 0, 1);
	glVertexPointer(3, GL_FLOAT,         stride, (vbo ? (void *)0             : &v));
	glColorPointer (4, GL_UNSIGNED_BYTE, stride, (vbo ? (void *)sizeof(point) : &c));
}


void draw_unit_sphere(int ndiv, bool do_texture) {

#if 0
	assert(quadric);
	if (do_texture) gluQuadricTexture(quadric, GL_TRUE);
	gluSphere(quadric, 1.0, ndiv, ndiv);
	if (do_texture) gluQuadricTexture(quadric, GL_FALSE);
#else
	draw_sphere_dlist_raw(ndiv, do_texture);
#endif
}


struct wap_obj {

	bool is_shadowed;
	int id, ndiv;
	wap_obj(int id_, int ndiv_, bool shadowed) : is_shadowed(shadowed), id(id_), ndiv(ndiv_) {}
};


void draw_obj(obj_group &objg, vector<wap_obj> *wap_vis_objs, int type, float radius, const colorRGBA &color,
			  int ndiv, int j, bool is_shadowed, bool in_ammo) {

	float const cd_scale(NDIV_SCALE*window_width);
	dwobject const &obj(objg.get_obj(j));
	point const &pos(obj.pos);
	bool const cull_face(get_cull_face(type, color));
	if (cull_face) glEnable(GL_CULL_FACE);

	switch (type) {
	case SMILEY:
		if (!(obj.flags & CAMERA_VIEW)) {
			draw_smiley(pos, obj.orientation, radius, ndiv, obj.time, obj.health, j, is_shadowed,
				(in_ammo ? NULL : &objg.get_td()->get_mesh(j)));
		}
		draw_weapon_in_hand(j); // Note: view culling doesn't use correct bounding sphere for all weapons
		break;
	case SFPART:
		draw_smiley_part(pos, pos, obj.orientation, obj.direction, 1, ndiv, is_shadowed);
		break;
	case CHUNK:
		draw_chunk(pos, radius, obj.init_dir, obj.vdeform, (obj.flags & TYPE_FLAG), ndiv, is_shadowed);
		break;
	case SKULL:
		draw_skull(pos, obj.orientation, radius, obj.status, ndiv);
		break;
	case ROCKET:
		draw_rocket(pos, obj.init_dir, radius, obj.type, ndiv, obj.time, is_shadowed);
		break;
	case SEEK_D:
		draw_seekd(pos, obj.init_dir, radius, obj.type, ndiv, is_shadowed);
		break;
	case LANDMINE:
		draw_landmine(pos, radius, ndiv, obj.time, obj.source, is_shadowed, in_ammo);
		break;
	case PLASMA:
		draw_plasma(pos, pos, radius, (in_ammo ? 1.0 : obj.init_dir.x), ndiv, 1, !in_ammo, obj.time);
		break;
	case GRENADE:
		draw_grenade(pos, obj.init_dir, radius, ndiv, (in_ammo ? 0 : obj.time), is_shadowed, in_ammo, 0);
		break;
	case CGRENADE:
		draw_grenade(pos, obj.init_dir, radius, ndiv, (in_ammo ? 0 : obj.time), is_shadowed, in_ammo, 1);
		break;
	case BALL:
		// FIXME: this is the only place where drawing an object modifies its physics state, but it's difficult to move the code
		draw_rolling_obj(pos, objg.get_obj(j).init_dir, radius, obj.status, ndiv, ((obj.flags & PLATFORM_COLL) != 0),
			dodgeball_tids[(game_mode == 2) ? (j%NUM_DB_TIDS) : 0], (in_ammo ? NULL : &objg.get_td()->get_matrix(j)));
		break;
	case POWERUP:
	case HEALTH:
	case SHIELD:
		draw_powerup(pos, radius, ndiv, ((type == POWERUP) ? (int)obj.direction : -1), color, is_shadowed);
		break;
	case WA_PACK:
		if (wid_need_weapon((int)obj.direction)) {
			wap_vis_objs[0].push_back(wap_obj(j, ndiv, is_shadowed));
		}
		else {
			wap_vis_objs[1].push_back(wap_obj(j, ndiv, is_shadowed));
		}
		break;
	case WEAPON:
		wap_vis_objs[0].push_back(wap_obj(j, ndiv, is_shadowed));
		break;
	case AMMO:
		wap_vis_objs[1].push_back(wap_obj(j, ndiv, is_shadowed));
		break;
	default:
		if (obj.vdeform != all_ones) {
			glPushMatrix();
			translate_to(pos);
			scale_by(obj.vdeform);
			rotate_into_plus_z(obj.orientation); // might be unnesessary
			gluSphere(quadric, radius, ndiv, ndiv);
			glPopMatrix();
		}
		else {
			draw_sphere_dlist(pos, radius, ndiv, 0);
		}
	}
	if (cull_face) glDisable(GL_CULL_FACE);
}


bool is_object_shadowed(dwobject &obj, bool calc_shadow, float cd_scale, float radius, int light, unsigned &num_shadow_test) {

	bool is_shadowed((obj.flags & SHADOWED) != 0); // previous value

	if (calc_shadow) {
		float const pt_size(cd_scale/distance_to_camera(obj.pos)); // approx pixel size
		int const skipval(min(20, int(8.0/pt_size)));

		if (skipval <= 1 || (obj.time % skipval) == 0) {
			is_shadowed = pt_is_shadowed(obj.pos, light, obj.status, radius, obj.coll_id, 0, (pt_size < 2.0));
			if (is_shadowed) obj.flags |= SHADOWED; else obj.flags &= ~SHADOWED;
			++num_shadow_test;
		}
	}
	return is_shadowed;
}


void draw_group(obj_group &objg) {

	RESET_TIME;
	int const light(get_specular_light());
	float specular_brightness(0.5*brightness);
	set_specular(0.0, 1.0); // disable
	colorRGBA color2;
	set_fill_mode();
	glEnable(GL_NORMALIZE);
	int const type(objg.get_ptype());
	obj_type const &otype(object_types[type]);
	int tid(otype.tid), last_shadowed(-1);
	float const radius(otype.radius), cd_scale(NDIV_SCALE*radius*window_width);
	unsigned const flags(otype.flags);
	bool do_texture(select_texture(tid));
	colorRGBA color(otype.color), tcolor(color);
	set_color(color);
	gluQuadricTexture(quadric, do_texture);
	check_drawing_flags(flags, 1, 0);
	int const clip_level((type == SMILEY || type == LANDMINE || type == ROCKET || type == BALL) ? 2 : 0);
	bool const calc_shadow(color != BLACK || (otype.flags & (SPECULAR | LOW_SPECULAR)));
	unsigned num_drawn(0), num_shadow_test(0);

	if (type == LEAF) { // leaves
		int last_tid(-1);
		set_lighted_sides(2);
		enable_blend();
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.75);
		glNormal3f(0.0, 1.0, 0.0);
		//set_leaf_shader(0.75); // Note: needs colors, but we set a/d lighting for shadows
		static vector<pair<unsigned, unsigned> > ordering;
		ordering.resize(0);
		ordering.reserve(objg.end_id);

		for (unsigned j = 0; j < objg.end_id; ++j) {
			dwobject const &obj(objg.get_obj(j));
			if (obj.disabled()) continue;
			float const scale(obj.init_dir.z);
			assert(scale > 0.0);
			if (!sphere_in_camera_view(obj.pos, 4.0*scale*leaf_size, clip_level)) continue;
			int const tree_type(obj.source);
			assert(tree_type >= 0 && tree_type < NUM_TREE_TYPES);
			int const tid(tree_types[tree_type].leaf_tex);
			assert(tid < NUM_TEXTURES);
			ordering.push_back(make_pair(tid, j));
		}
		num_drawn += ordering.size();
		sort(ordering.begin(), ordering.end()); // sort by texture id

		for (unsigned j = 0; j < ordering.size(); ++j) {
			dwobject &obj(objg.get_obj(ordering[j].second));
			float const scale(obj.init_dir.z), lsize(scale*leaf_size);
			int const tid(ordering[j].first);
			
			if (draw_model == 0 && tid != last_tid) {
				select_texture(tid);
				last_tid = tid;
			}
			point pos(obj.pos);
			if (place_obj_on_grass(pos, lsize)) pos.z = 0.5*(obj.pos.z + pos.z-lsize); // leaf is partially on grass
			glPushMatrix();
			translate_to(pos);
			uniform_scale(scale);
			rotate_about(obj.angle, obj.orientation);
			glRotatef(TO_DEG*obj.init_dir.x, 0.0, 0.0, 1.0);

			float const t(((float)obj.time)/((float)otype.lifetime));
			colorRGBA const dry_color(1.0, 0.7, 0.1); // this is the final color, even for partially burnt leaves - oh well
			colorRGBA leaf_color(WHITE);
			UNROLL_3X(leaf_color[i_] *= obj.vdeform[i_];) // vdeform.x is color_scale
			if (leaf_color != BLACK) blend_color(leaf_color, dry_color, leaf_color, t, 0);
			bool const shadowed(is_object_shadowed(obj, calc_shadow, cd_scale, radius, light, num_shadow_test));
			if (shadowed) set_specular(0.0, 1.0); else set_specular(0.1, 10.0); // FIXME: should leaves on trees be a matching specular again?
			set_shadowed_color(leaf_color, obj.pos, shadowed, 0);
			glBegin(GL_QUADS);

			for (unsigned k = 0; k < 4; ++k) {
				glTexCoord2f(float(k>>1), float(k==0||k==3));
				leaf_points[k].do_glVertex();
			}
			glEnd();
			glPopMatrix();
		} // for j
		//unset_shader_prog();
		//glDisable(GL_TEXTURE_2D);
		disable_blend();
		set_specular(0.0, 1.0);
		glDisable(GL_ALPHA_TEST);
		set_lighted_sides(1);
	} // leaf
	else if (objg.large_radius()) { // large objects
		vector<wap_obj> wap_vis_objs[2];
		bool const gm_smiley(game_mode && type == SMILEY);
		float const radius_ext(gm_smiley ? 2.0*radius : radius); // double smiley radius to account for weapons

		for (unsigned j = 0; j < objg.end_id; ++j) {
			dwobject &obj(objg.get_obj(j));
			if (obj.disabled() || ((obj.flags & CAMERA_VIEW) && type != SMILEY)) continue;
			point const &pos(obj.pos);
			if (!sphere_in_camera_view(pos, radius_ext, clip_level)) continue;
			bool const is_shadowed(is_object_shadowed(obj, calc_shadow, cd_scale, radius, light, num_shadow_test));

			if ((int)is_shadowed != last_shadowed) {
				set_obj_specular(is_shadowed, flags, specular_brightness); // slow?
				last_shadowed = is_shadowed;
			}
			if (DEBUG_COLORCODE) {
				set_color_by_status(obj.status, pos, is_shadowed);
			}
			else if (type != SMILEY && type != SFPART && type != ROCKET && type != CHUNK &&
				type != LANDMINE && type != PLASMA && type != POWERUP && type != HEALTH && type != SHIELD)
			{
				set_shadowed_color(color, pos, is_shadowed, 0);
			}
			++num_drawn;
			float const pt_size(cd_scale/distance_to_camera(pos));
			int const ndiv(min(N_SPHERE_DIV, max(3, int(min(pt_size, 3.0f*sqrt(pt_size))))));
			draw_obj(objg, wap_vis_objs, type, radius, color, ndiv, j, is_shadowed, 0);
		} // for j
		for (unsigned k = 0; k < wap_vis_objs[0].size(); ++k) { // draw weapons
			draw_weapon2(objg.get_obj(wap_vis_objs[0][k].id), radius);
		}
		for (unsigned k = 0; k < wap_vis_objs[1].size(); ++k) { // draw ammo
			unsigned const j(wap_vis_objs[1][k].id);
			assert(j < objg.max_objects());
			draw_ammo(objg, radius, color, wap_vis_objs[1][k].ndiv, j, wap_vis_objs[1][k].is_shadowed);
		}
		if (!wap_vis_objs[0].empty() || !wap_vis_objs[1].empty()) {
			check_drawing_flags(otype.flags, 1, 0);
			if (!select_texture(tid)) glDisable(GL_TEXTURE_2D);
			gluQuadricTexture(quadric, do_texture);
		}
		for (unsigned j = 0; j < 2; ++j) {
			for (unsigned k = 0; k < wap_vis_objs[j].size(); ++k) {
				wap_obj const &wa(wap_vis_objs[j][k]);
				set_obj_specular(wa.is_shadowed, flags, specular_brightness);
				dwobject const &obj(objg.get_obj(wa.id));
				set_shadowed_color(color, obj.pos, wa.is_shadowed, 0);
				draw_subdiv_sphere(obj.pos, radius, wa.ndiv, 0, 0);
			}
		}
	} // large objects
	else { // small objects
		bool const precip((objg.flags & PRECIPITATION) != 0);

		switch (type) { // pre-draw
		case SHRAPNEL:
			glDisable(GL_LIGHTING);
			glBegin(GL_TRIANGLES);
			break;
		case PARTICLE:
			glDisable(GL_LIGHTING);
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.01);
			glBegin(GL_QUADS);
			break;
		case SNOW:
			glDepthMask(GL_FALSE);
			break;
		}
		for (unsigned j = 0; j < objg.end_id; ++j) {
			dwobject &obj(objg.get_obj(j));
			point const &pos(obj.pos);
			if (obj.disabled() || (obj.flags & CAMERA_VIEW))      continue;
			float const tradius(obj.get_true_radius()); // differs from radius for fragments
			if (!sphere_in_camera_view(pos, tradius, clip_level)) continue;
			++num_drawn;
			bool const is_shadowed(is_object_shadowed(obj, calc_shadow, cd_scale, tradius, light, num_shadow_test));

			if ((int)is_shadowed != last_shadowed) {
				set_obj_specular(is_shadowed, flags, specular_brightness); // slow?
				last_shadowed = is_shadowed;
			}
			if (type == FRAGMENT) {
				tid = -obj.coll_id - 2; // should we sort fragments by texture id?
				do_texture = select_texture(tid);
				if (!do_texture) glDisable(GL_TEXTURE_2D);
				UNROLL_3X(color2[i_] = obj.init_dir[i_];)
				color2.alpha = obj.vdeform.y;
			}
			else if (do_texture && type != SNOW) {
				color2 = WHITE;
			}
			else {
				color2 = object_types[obj.type].color;
			}
			if (type != SHRAPNEL && type != PARTICLE) {
				if (type == DROPLET) select_liquid_color(color2, get_ypos(pos.x), get_ypos(pos.y));
				scale_color_uw(color2); // ???

				if (do_texture) {
					assert(tid >= 0);
					tcolor = texture_color(tid);
					UNROLL_3X(tcolor[i_] *= color2[i_];)
				}
				else {
					tcolor = color2;
				}
			}
			switch (type) {
			case SHELLC:
				set_color_v2(color2, pos, obj.status, is_shadowed, precip);
				draw_shell_casing(pos, obj.orientation, obj.init_dir, tradius, obj.angle, cd_scale, is_shadowed, obj.direction);
				break;
			case SHRAPNEL:
				draw_shrapnel(obj, tradius, is_shadowed);
				break;
			case STAR5:
				set_color_v2(color2, pos, obj.status, is_shadowed, precip);
				draw_star(pos, obj.orientation, obj.init_dir, tradius, obj.angle, 1);
				break;
			case PARTICLE:
				draw_particle(obj, tradius);
				break;

			case SAND:
			case DIRT:
			case ROCK:
				color2 *= obj.orientation.y;
				if (do_texture) tcolor *= obj.orientation.y;
				draw_sized_point(obj, tradius, obj.orientation.x*cd_scale, color2, tcolor, do_texture, is_shadowed, (type == DIRT || type == ROCK));
				break;

			case FRAGMENT: // draw_fragment()?
				if (obj.vdeform.z > 0.0) { // shatterable - use triangle
					set_color_v2(color2, pos, obj.status, is_shadowed, 0);
					set_lighted_sides(2);
					glBegin(GL_TRIANGLES);
					draw_rotated_triangle(pos, obj.orientation, tradius, obj.angle, (do_texture ? obj.vdeform.z : 0.0)); // obj.vdeform.z = tscale
					glEnd();
					set_lighted_sides(1);
					break;
				}
				draw_sized_point(obj, tradius, cd_scale, color2, tcolor, do_texture, is_shadowed, 2);
				break;

			default:
				if (DEBUG_COLOR_COLLS) {
					int cindex;
					float const time(TIMESTEP*fticks);
					point const pos2(pos + obj.velocity*time - point(0.0, 0.0, -base_gravity*GRAVITY*time*time*otype.gravity));
					set_color(check_coll_line(pos, pos2, cindex, -1, 0, 0) ? RED : GREEN);
					draw_line(pos, pos2);
				}
				draw_sized_point(obj, tradius, cd_scale, color2, tcolor, do_texture, is_shadowed, 0);
			} // switch (type)
		} // for j
		switch (type) { // post-draw
		case SHRAPNEL:
			glEnd();
			glEnable(GL_LIGHTING);
			glDisable(GL_ALPHA_TEST);
			break;
		case PARTICLE:
			glEnd();
			glEnable(GL_LIGHTING);
			glDisable(GL_ALPHA_TEST);
			break;
		case SNOW:
			if (!snow_pld.empty()) { // draw snowflakes from points in a custom geometry shader
				setup_enabled_lights();
				set_shader_prefix("vec4 apply_fog(in vec4 color) {return color;}", 1); // add pass-through fog implementation for FS
				set_shader_prefix("#define USE_LIGHT_COLORS", 0); // VS
				unsigned const p(set_shader_prog("ads_lighting.part*+two_lights_no_xform", "simple_texture", "pt_billboard_tri", GL_POINTS, GL_TRIANGLE_STRIP, 3));
				add_uniform_float(p, "size", 2.0*radius); // Note: size no longer depends on angle
				add_uniform_int(p, "tex0", 0);
				snow_pld.draw_and_clear();
				unset_shader_prog();
			}
			glDepthMask(GL_TRUE);
			break;
		}
		glDisable(GL_TEXTURE_2D);
		obj_pld.draw_and_clear();
	} // small object
	check_drawing_flags(flags, 0, 0);
	gluQuadricTexture(quadric, GL_FALSE);
	glDisable(GL_TEXTURE_2D);

	if (SHOW_DRAW_TIME) {
		cout << "type = " << objg.type << ", num = " << objg.end_id << ", drawn = " << num_drawn << ", shadow tests: " << num_shadow_test << " ";
		PRINT_TIME("Group");
	}
}


void draw_sized_point(dwobject const &obj, float radius, float cd_scale, const colorRGBA &color, const colorRGBA &tcolor,
					  bool do_texture, bool is_shadowed, int is_chunky)
{
	point pos(obj.pos);
	point const camera(get_camera_pos());
	float point_dia(cd_scale/p2p_dist(camera, pos));
	if (do_zoom) point_dia *= ZOOM_FACTOR;
	int const type(obj.type);
	bool const draw_large(point_dia >= 2.5);
	bool const draw_snowflake(draw_large && type == SNOW);
	bool const tail_type((object_types[type].flags & TAIL_WHEN_FALL) != 0);
	bool const tail(tail_type && obj.status == 1 && obj.velocity.z < RAIN_TAIL_MIN_V && !(obj.flags & OBJ_COLLIDED));

	if (tail && !draw_large) { // draw rain with lines
		point pos2(pos);
		pos2.z -= 2.0*fticks*TIMESTEP*obj.velocity.z;
		colorRGBA color2(color);
		color2.alpha *= min(1.0f, 0.5f*point_dia);
		obj_pld.add_line(pos2, (camera - pos2), ALPHA0, pos, (camera - pos), color2);
		return;
	}
	bool const precip((object_types[type].flags & IS_PRECIP) != 0);
	bool const is_drop(type != SNOW && (object_types[type].flags & OBJ_IS_DROP)); // can get snow when changing temp
	if (!precip && point_dia < 0.4) return; // clip it

	if (is_drop && obj.status == 4 && (obj.flags & STATIC_COBJ_COLL)) { // draw as puddle
		if (!draw_large) return; // skip
		assert(!do_texture);
		colorRGBA color2(color);
		if (type == RAIN) color2.alpha *= 0.5; // rain is mostly transparent when small
		set_color_v2(color2, pos, obj.status, is_shadowed, precip);
		glDepthMask(GL_FALSE);
		select_texture(BLUR_TEX);
		glNormal3f(0.0, 0.0, 1.0);
		glBegin(GL_QUADS);
		draw_billboard(pos, (pos + plus_z), vector3d(1.0, 0.0, 0.0), 5.0*radius, 5.0*radius);
		glEnd();
		glDisable(GL_TEXTURE_2D);
		glDepthMask(GL_TRUE);
		return;
	}
	if (!draw_large || draw_snowflake) { // draw as a point
		colorRGBA a(do_texture ? tcolor : color);
		get_shadowed_color(a, pos, is_shadowed, precip, 0);
		bool const scatters(type == RAIN || type == SNOW);
		vector3d const n(is_shadowed ? (pos - get_light_pos()) : ((scatters ? get_light_pos() : camera) - pos));
		if (draw_snowflake) snow_pld.add_pt(pos, n, a); else obj_pld.add_pt(pos, n, a);
		return;
	}
	colorRGBA color_l(color);
	set_color_v2(color_l, pos, obj.status, is_shadowed, precip);
	bool const cull_face(get_cull_face(type, color));
	glPushMatrix();

	if (cull_face) {
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
	}

	// draw as a sphere
	if (is_chunky) {
		assert(!tail);
		vector3d const v((is_chunky == 2) ? obj.orientation : obj.init_dir);
		int const ndiv(max(3, int(3 + 1.5*(v.x + v.y + v.z))));
		translate_to(pos);
		vector3d const scale((0.8+0.5*fabs(v.x)), (0.8+0.5*fabs(v.y)), (0.8+0.5*fabs(v.z)));
		scale_by(scale*radius);
		glRotatef(360.0*(v.x - v.y), v.x, v.y, (v.z+0.01));
		draw_unit_sphere(ndiv, do_texture);
		glTranslatef(0.1*(v.x-v.y), 0.1*(v.y-v.z), 0.1*(v.x-v.z));
		glRotatef(360.0*(v.z - v.x), v.y, v.z, (v.x+0.01));
		draw_unit_sphere(ndiv, do_texture);
	}
	else {
		int ndiv(int(4.0*sqrt(point_dia)));

		if (is_droplet(type)) {
			ndiv = min(ndiv/2, N_SPHERE_DIV/2);
		}
		else if (type == ROCK || type == SAND || type == DIRT || type == FRAGMENT) {
			ndiv /= 2;
		}
		ndiv = max(4, min(ndiv, N_SPHERE_DIV));
	
		if (quadric != 0 && ndiv > 3 && tail) { // cone on the tail of the raindrop
			translate_to(pos);
			gluCylinder(quadric, radius, 0.0, 2.5*radius, (ndiv>>1), 1);
			glTranslatef(0.0, 0.0, -0.6*radius);
			glScalef(1.0, 1.0, 2.0);
			pos = all_zeros;
		}
		draw_sphere_dlist(pos, radius, ndiv, do_texture);
	}
	if (cull_face) glDisable(GL_CULL_FACE);
	glPopMatrix();
}


void draw_weapon2(dwobject const &obj, float radius) {

	int const wid((int)obj.direction);
	vector3d d(-1.0, 0.0, 0.0), v(obj.pos);
	if (wid != W_BLADE) v.x += 0.7*radius;
	draw_weapon_simple(v, d, 0.5*radius, obj.coll_id, wid, 0.25);
}


void draw_ammo(obj_group &objg, float radius, const colorRGBA &color, int ndiv, int j, bool is_shadowed) {

	dwobject const &obj(objg.get_obj(j));
	point pos(obj.pos);
	vector<wap_obj> wap_vis_objs[2]; // not actually used
	int const atype(get_ammo_or_obj((int)obj.direction));

	if (atype >= 0) {
		check_drawing_flags(object_types[atype].flags, 1, is_shadowed);
		int const tex(select_texture(object_types[atype].tid));
		if (!tex) glDisable(GL_TEXTURE_2D);
		gluQuadricTexture(quadric, tex);
		set_shadowed_color(object_types[atype].color, pos, is_shadowed);
		bool const cull_face(get_cull_face(atype, color));
		if (cull_face) glEnable(GL_CULL_FACE);

		switch (atype) {
		case SHELLC: // M16
			pos.z -= 0.5*radius;
			draw_cylinder(pos, 1.0*radius, 0.2*radius, 0.2*radius, ndiv, 1, 1);
			pos.z += radius;
			draw_subdiv_sphere(pos, 0.2*radius, ndiv, 0, 0);
			break;
		case PROJECTILE: // shotgun
			for (unsigned n = 0; n < 2; ++n) { // two shells in one ammo
				point pos2(pos);
				pos2.x += (1.0 - 2.0*n)*0.3*radius;
				set_shadowed_color(RED, pos2, is_shadowed);
				pos2.z -= 0.5*radius;
				draw_cylinder(pos2, 1.2*radius, 0.3*radius, 0.3*radius, ndiv, 1, 1);
				set_shadowed_color(GOLD, pos2, is_shadowed);
				pos2.z -= 0.2*radius;
				draw_cylinder(pos2, 0.4*radius, 0.32*radius, 0.32*radius, ndiv, 1, 1);
			}
			break;
		case BEAM: // laser
			set_shadowed_color(RED, pos, is_shadowed);
			pos.z -= 0.5*radius;
			draw_cylinder(pos, 1.0*radius, 0.1*radius, 0.1*radius, ndiv, 1, 1);
			break;
		case STAR5: // throwing star
			draw_star(pos, obj.orientation, obj.init_dir, 0.4*radius, obj.angle, 0);
			break;
		case GASSED:
			draw_subdiv_sphere(pos, 0.6*radius, ndiv, 1, 0);
			break;
		default:
			draw_obj(objg, wap_vis_objs, atype, 0.4*radius, color, ndiv, j, is_shadowed, 1);
		}
		if (cull_face) glDisable(GL_CULL_FACE);
	}
}


colorRGBA get_powerup_color(int powerup) {

	switch (powerup) {
	case PU_DAMAGE:       return CYAN;
	case PU_REGEN:        return BLUE;
	case PU_SHIELD:       return GREEN;
	case PU_SPEED:        return WHITE;
	case PU_FLIGHT:       return PURPLE;
	case PU_INVISIBILITY: return BLACK; // not useful for smileys
	}
	return BLACK;
}


inline void rotate_to_dir(vector3d const &dir) { // normalized to +y (for smileys)

	glRotatef(atan2(dir.y, dir.x)*TO_DEG-90.0, 0.0, 0.0, 1.0);
	glRotatef(safe_acosf(-dir.z)*TO_DEG-90.0, 1.0, 0.0, 0.0);
}


void draw_smiley_part(point const &pos, point const &pos0, vector3d const &orient, int type,
					  int use_orient, int ndiv, bool is_shadowed, float scale)
{
	assert(type < NUM_SMILEY_PARTS);
	float const radius(scale*object_types[SFPART].radius);
	colorRGBA const sf_color[NUM_SMILEY_PARTS] = {BLACK, RED, PINK};
	set_shadowed_color(sf_color[type], pos0, is_shadowed);

	switch (type) {
	case SF_EYE:
		draw_sphere_at(pos, 1.0*radius, ndiv);
		break;
	case SF_NOSE:
		draw_sphere_at(pos, 1.2*radius, ndiv);
		break;
	case SF_TONGUE:
		glPushMatrix();
		translate_to(pos);
		if (use_orient) rotate_to_dir(orient);
		glScalef(1.0, 2.0, 0.25);
		draw_sphere_dlist(all_zeros, 1.5*radius, ndiv, 0);
		glPopMatrix();
		break;
	default: assert(0);
	}
}


colorRGBA mult_alpha(colorRGBA const &c, float alpha) {
	return colorRGBA(c.red, c.green, c.blue, c.alpha*alpha);
}


void draw_smiley(point const &pos, vector3d const &orient, float radius, int ndiv, int time,
				 float health, int id, bool is_shadowed, mesh2d const *const mesh)
{
	colorRGBA color;
	int const powerup(sstates[id].powerup), ndiv2(max(3, (ndiv>>1)));
	float const dist(distance_to_camera(pos));
	glPushMatrix();
	translate_to(pos);
	rotate_to_dir(orient);
	//uniform_scale(radius);
	point pos2(-0.4*radius, 0.85*radius, 0.3*radius);

	// draw eyes
	for (unsigned i = 0; i < 2; ++i) {
		if (health > 10.0) {
			float const scale((powerup == PU_SPEED) ? 1.5 : 1.0);
			point const pos3 ((powerup == PU_SPEED) ? (pos2 + point(0.0, 0.2*radius, 0.0)) : pos2);
			draw_smiley_part(pos3, pos, orient, SF_EYE, 0, ndiv2, is_shadowed, scale); // eyes
		}
		else {
			set_shadowed_color(BLACK, pos, is_shadowed);
			enable_blend();
			glLineWidth(min(8.0f, max(1.0f, 6.0f/dist)));
			glPushMatrix();
			translate_to(pos2);
			uniform_scale(radius);
			glBegin(GL_LINES);

			for (unsigned l = 0; l < 2; ++l) {
				for (unsigned p = 0; p < 2; ++p) {
					glVertex3f((p ? -0.12 : 0.12), 0.1, ((p^l) ? -0.12 : 0.12));
				}
			}
			glEnd();
			glPopMatrix();
			glLineWidth(1.0);
			disable_blend();
		}
		pos2.x *= -1.0;
	}

	// draw nose
	if (powerup != PU_INVISIBILITY || same_team(id, -1)) { // show nose even if invisible if same team as player
		point pos3(0.0, 1.1*radius, 0.0);
		draw_smiley_part(pos3, pos, orient, SF_NOSE, 0, ndiv2, is_shadowed); // nose
	}
	float alpha(1.0);

	switch (powerup) {
		case PU_DAMAGE: // devil horns
			set_shadowed_color(RED, pos, is_shadowed);
			glPushMatrix();
			glTranslatef( 0.3*radius, 0.7*radius, 0.6*radius);
			draw_cylinder(0.6*radius, 0.1*radius, 0.0, ndiv2, 1, 0);
			glTranslatef(-0.6*radius, 0.0, 0.0);
			draw_cylinder(0.6*radius, 0.1*radius, 0.0, ndiv2, 1, 0);
			glPopMatrix();
			break;

		case PU_REGEN: // raindrops
			if (animate2) {
				int const cid(coll_id[WDROPLET]), k(obj_groups[cid].choose_object());
				obj_groups[cid].create_object_at(k, (pos + point(0.0, 0.0, 4.0*radius)));
				vadd_rand(obj_groups[cid].get_obj(k).pos, 0.04);
			}
			break;

		case PU_SHIELD: // shield + gas mask
			// gas generated in gameplay.cpp::update_game_frame()
			break;

		case PU_SPEED:
			if (animate2 && !(rand()&3)) gen_smoke(pos);
			break;

		case PU_FLIGHT: // propeller or wings?
			set_shadowed_color(BLACK, pos, is_shadowed);
			glPushMatrix();
			glTranslatef(0.0, 0.0, 0.9*radius);
			draw_cylinder(0.5*radius, 0.05*radius, 0.05*radius, ndiv2, 1, 0);
			glTranslatef(0.0, 0.0, 0.5*radius);
			glRotatef(float((30*time)%360), 0.0, 0.0, 1.0);
			glScalef(1.0, 0.25, 0.05);
			glTranslatef( 0.5*radius, 0.0, 0.0);
			draw_sphere_dlist(all_zeros, 0.5*radius, ndiv, 0); // propeller
			glTranslatef(-1.0*radius, 0.0, 0.0);
			draw_sphere_dlist(all_zeros, 0.5*radius, ndiv, 0); // propeller
			glPopMatrix();
			break;

		case PU_INVISIBILITY:
			{
				float const put(float(sstates[id].powerup_time)/TICKS_PER_SECOND), init_put(float(POWERUP_TIME)/TICKS_PER_SECOND);

				if ((init_put - put) < 1.0) { // fading out
					alpha = (1.0 - (init_put - put));
				}
				else if (put < 1.0) { // fading in
					alpha = (1.0 - put);
				}
				else { // fully invisible
					glPopMatrix();
					return;
				}
				if (alpha < 1.0) enable_blend();
				break;
			}
	} // switch (powerup)
	if (powerup != PU_INVISIBILITY && time%10 < 5 && powerup >= 0) { // powerup
		set_shadowed_color(get_powerup_color(powerup), pos, is_shadowed);
	}
	else if (health >= 50.0) {
		set_shadowed_color(mult_alpha(YELLOW, alpha), pos, is_shadowed);
	}
	else {
		set_shadowed_color(colorRGBA(1.0, (0.25 + 0.015*health), 0.0, alpha), pos, is_shadowed);
	}
	if (game_mode == 2) { // dodgeball
		select_texture(CAMOFLAGE_TEX);
	}
	else {
		select_smiley_texture(id);
	}
	if (mesh) { // main body
		mesh->draw_perturbed_sphere(all_zeros, radius, ndiv, 1);
		//if (mesh->size > 0) ndiv = mesh->size;
	}
	else {
		draw_sphere_dlist(all_zeros, radius, ndiv, 1);
	}
	glDisable(GL_TEXTURE_2D);

	if (teams > 1) { // draw team headband
		set_shadowed_color(mult_alpha(get_smiley_team_color(id), alpha), pos, is_shadowed);
		glPushMatrix();
		glTranslatef(0.0, 0.0, 0.45*radius);
		glScalef(1.0, 1.0, 0.5);
		gluSphere(quadric, 0.94*radius, ndiv, ndiv2);
		glPopMatrix();
	}
	// draw unique identifier
	int temp(teams);
	teams = 10; // 10 default colors
	set_shadowed_color(mult_alpha(get_smiley_team_color(id+1), alpha), pos, is_shadowed);
	teams = temp;
	glPushMatrix();
	glTranslatef(0.0, 0.0, 0.8*radius);
	glScalef(1.0, 1.0, 0.3);
	gluSphere(quadric, 0.65*radius, ndiv, ndiv2);
	glPopMatrix();
	
	// draw mouth
	float const hval(0.004*(100.0 - min(160.0f, health)));
	set_shadowed_color(mult_alpha(BLACK, alpha), pos, is_shadowed);
	enable_blend();
	glLineWidth(min(8.0f, max(1.0f, 5.0f/dist)));
	glPushMatrix();
	uniform_scale(radius);
	glBegin(GL_LINE_STRIP);
	glVertex3f(-0.5,  0.95, -0.2-hval);
	glVertex3f(-0.15, 0.95, -0.4);
	glVertex3f( 0.15, 0.95, -0.4);
	glVertex3f( 0.5,  0.95, -0.2-hval);
	glEnd();
	glPopMatrix();
	glLineWidth(1.0);
	disable_blend();

	// draw tongue
	if (sstates[id].kill_time < int(2*TICKS_PER_SECOND) || powerup == PU_DAMAGE) { // stick your tongue out at a dead enemy
		point pos4(0.0, 0.8*radius, -0.4*radius);
		draw_smiley_part(pos4, pos, orient, SF_TONGUE, 0, ndiv2, is_shadowed);
	}
	if (game_mode == 2 && (sstates[id].p_ammo[W_BALL] > 0 || UNLIMITED_WEAPONS)) { // dodgeball
		select_texture(select_dodgeball_texture(id));
		set_shadowed_color(mult_alpha(object_types[BALL].color, alpha), pos, is_shadowed);
		draw_sphere_dlist(point(0.0, 1.3*radius, 0.0), 0.8*object_types[BALL].radius, ndiv, 1);
		glDisable(GL_TEXTURE_2D);
	}
	glPopMatrix();
	vector3d hit_dir;
	int const hit(get_smiley_hit(hit_dir, id));

	if (hit > 0) { // hit - draw damage or shields
		select_texture(SBLUR_TEX);
		enable_blend();
		colorRGBA color2((sstates[id].shields < 0.01) ? BLOOD_C : GREEN);
		color2.alpha = alpha*hit/6.0;
		set_shadowed_color(color2, pos, is_shadowed);
		glPushMatrix();
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.05);
		translate_to(pos);
		rotate_sphere_tex_to_dir(hit_dir);
		draw_sphere_dlist(all_zeros, 1.015*radius, ndiv, 1);
		glDisable(GL_ALPHA_TEST);
		glPopMatrix();

		if (powerup == PU_SHIELD) {
			select_texture(PLASMA_TEX);
			set_color(mult_alpha(PURPLE, alpha)); // not scaled
			draw_sphere_dlist(pos, 1.05*radius, ndiv, 1);
		}
		disable_blend();
		glDisable(GL_TEXTURE_2D);
	}
	if (alpha < 1.0) disable_blend();
}


void draw_powerup(point const &pos, float radius, int ndiv, int type, const colorRGBA &color, bool is_shadowed) {

	colorRGBA const color2((type == -1) ? color : get_powerup_color(type));
	glDisable(GL_LIGHTING);
	color2.do_glColor();
	draw_subdiv_sphere(pos, 0.7*radius, ndiv, 0, 0); // draw flare/billboard?
	glEnable(GL_LIGHTING);
	set_shadowed_color(color, pos, is_shadowed);
	draw_subdiv_sphere(pos, radius, ndiv, 0, 0);
}


void draw_rolling_obj(point const &pos, point &lpos, float radius, int status, int ndiv, bool on_platform, int tid, xform_matrix *matrix) {

	select_texture(tid);
	glPushMatrix();
	translate_to(pos);
	
	if (matrix) {
		if (on_platform) lpos = pos; // reset now so there's no rotation
		apply_obj_mesh_roll(*matrix, pos, lpos, radius, ((status == 1) ? 0.01 : 0.0), ((status == 1) ? 0.2 : 1.0));
	}
	draw_sphere_dlist(all_zeros, radius, 2*ndiv, 1);
	glPopMatrix();
	lpos = pos;
}


void draw_skull(point const &pos, vector3d const &orient, float radius, int status, int ndiv) {

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.9);
	glPushMatrix();
	translate_to(pos);
	rotate_from_v2v(orient, vector3d(0.0, -1.0, 0.0));
	glRotatef(180.0, 0.0, 1.0, 0.0);
	draw_sphere_dlist_back_to_front(all_zeros, radius, 2*ndiv, 1);
	glPopMatrix();
	glDisable(GL_ALPHA_TEST);
}


void gen_rocket_smoke(point const &pos, vector3d const &orient, float radius) { // rocket and seekd

	if (animate2) {
		if (distance_to_camera_sq(pos) > 0.04 && iticks > rand()%3) {
			gen_smoke(pos + orient.get_norm()*(2.0*radius));
		}
		add_blastr(pos, orient, 2.0*radius, 0.0, 4, -2, WHITE, WHITE, ETYPE_ANIM_FIRE);
	}
}


void draw_rocket(point const &pos, vector3d const &orient, float radius, int type, int ndiv, int time, bool is_shadowed) {

	glPushMatrix();
	translate_to(pos);
	rotate_by_vector(orient, 0.0);
	set_shadowed_color(RED, pos, is_shadowed);
	uniform_scale(radius);
	glBegin(GL_TRIANGLES);
	glVertex3f( 0.0,  0.0,  0.0);
	glVertex3f( 1.8,  0.0, -2.0);
	glVertex3f(-1.8,  0.0, -2.0);
	glVertex3f( 0.0,  0.0,  0.0);
	glVertex3f( 0.0,  1.8, -2.0);
	glVertex3f( 0.0, -1.8, -2.0);
	glEnd();
	set_shadowed_color(object_types[ROCKET].color, pos, is_shadowed);
	glScalef(1.0, 1.0, -2.0);
	draw_unit_sphere(ndiv, 0);
	gluCylinder(quadric, 1.0, 1.0, 1.1, ndiv, 1);
	glPopMatrix();
	if (type == ROCKET) gen_rocket_smoke(pos, orient, radius);
}


void draw_seekd(point const &pos, vector3d const &orient, float radius, int type, int ndiv, bool is_shadowed) {

	assert(quadric);
	glPushMatrix();
	translate_to(pos);
	rotate_by_vector(orient, 0.0);
	uniform_scale(radius);
	glPushMatrix();
	glTranslatef(0.0, 0.0, -2.0);
	set_color(BLACK);
	gluCylinder(quadric, 1.0, 0.0, 1.0, ndiv, 1);
	glTranslatef(0.0, 0.0, -0.25);
	gluCylinder(quadric, 1.0, 1.0, 0.25, ndiv, 1);
	glPopMatrix();

	glScalef(1.0, 1.0, 1.5);
	glRotatef(90.0, -1.0, 0.0, 0.0);
	glRotatef(90.0,  0.0, 1.0, 0.0);
	set_shadowed_color(WHITE, pos, is_shadowed); // since seekd is black, is_shadowed may always be 0
	select_texture(SKULL_TEX);
	draw_unit_sphere(ndiv, 1);
	glDisable(GL_TEXTURE_2D);

	glPopMatrix();
	if (type == SEEK_D) gen_rocket_smoke(pos, orient, radius);
}


colorRGBA const &get_landmine_light_color(int time) {

	return ((time < 40) ? GREEN : (((time/6)&1) ? RED : BLUE));
}


float get_landmine_sensor_height(float radius, int time) {

	return ((time <= 6) ? 0 : ((time > 16) ? 1.5*radius : (radius + 0.05*radius*(time - 6))));
}


void draw_landmine(point pos, float radius, int ndiv, int time, int source, bool is_shadowed, bool in_ammo) {

	assert(radius > 0.0 && ndiv > 0);

	if (!in_ammo) {
		int const xpos(get_xpos(pos.x)), ypos(get_ypos(pos.y));

		if (!point_outside_mesh(xpos, ypos) && !is_mesh_disabled(xpos, ypos) &&
			pos.z < (interpolate_mesh_zval(pos.x, pos.y, 0.0, 0, 1) + 1.05*radius))
		{
			pos.z -= 0.8*radius; // appears to sink into the ground
		}
	}
	if (!DEBUG_COLORCODE) set_shadowed_color(WHITE, pos, is_shadowed);
	draw_subdiv_sphere(pos, radius, ndiv, 1, 0); // man body
	glDisable(GL_TEXTURE_2D);
	glPushMatrix();
	translate_to(pos);
	float const val(get_landmine_sensor_height(radius, time));

	if (time > 6) {
		set_shadowed_color(GRAY, pos, is_shadowed);
		gluCylinder(quadric, 0.05*radius, 0.05*radius, val, ndiv, 1);
		if (teams > 1) set_shadowed_color(get_smiley_team_color(source), pos, is_shadowed); // use team color
		gluDisk(quadric, 0, 0.05*radius, ndiv, 1); // sensor
	}
	pos.z += val;

	if (time > 20) {
		float val;
		glTranslatef(0.0, 0.0, 1.4*radius);
		glRotatef(((time > 26) ? 90.0 : 15.0*(time - 20)), 1.0, 0.0, 0.0);
		
		if (time > 36) {
			glRotatef(10.0*(time%36), 0.0, 1.0, 0.0);
			val = 0.42*radius;
		}
		else if (time > 26){
			val = 0.04*radius*(time - 26) + 0.02*radius;
		}
		if (time > 26) gluCylinder(quadric, 0.0, val, 0.4*radius, ndiv, 1); // sensor pole
	}
	glPopMatrix();

	if (time > 5) {
		pos.z += 0.15*radius;
		get_landmine_light_color(time).do_glColor();
		glDisable(GL_LIGHTING);
		draw_subdiv_sphere(pos, 0.15*radius, ndiv/2, 0, 0); // warning light
		glEnable(GL_LIGHTING);
	}
	if (glIsTexture(object_types[LANDMINE].tid)) glEnable(GL_TEXTURE_2D);
}


colorRGBA get_plasma_color(float size) {

	return colorRGBA(1.0, size/5.0, max(0.0f, 0.5f*(size-3.0f)), 0.9);
}


void draw_plasma(point const &pos, point const &part_pos, float radius, float size, int ndiv, int shpere_tex, bool gen_parts, int time) {

	int const tmode(shpere_tex ? GL_SPHERE_MAP : GL_EYE_LINEAR);
	colorRGBA const color(get_plasma_color(size + 0.5*(0.5 + 0.16*abs((time % 12) - 6))));

	if (animate2) {
		setup_texgen(0.2*rand_uniform(0.95, 1.05)/radius, 0.2*rand_uniform(0.95, 1.05)/radius, rand_float(), rand_float(), 0.0, tmode);
	}
	else {
		setup_texgen(0.2/radius, 0.2/radius, 0.0, 0.0, 0.0, tmode);
	}
	glDisable(GL_LIGHTING);
	color.do_glColor();
	if (animate2) radius *= rand_uniform(0.99, 1.01) + 0.1*(0.5 + 0.1*(abs((time % 20) - 10)));
	draw_sphere_dlist(pos, size*radius, ndiv, 1);
	glEnable(GL_LIGHTING);
	disable_texgen();
	if (gen_parts && animate2 && !is_underwater(part_pos, 1) && (rand()&15) == 0) gen_particles(part_pos, 1);
}


void draw_chunk(point const &pos, float radius, vector3d const &v, vector3d const &vdeform, int charred, int ndiv, bool is_shadowed) {

	ndiv    = min(ndiv, max(3, int(3 + 1.5*(v.x + v.y + v.z))));
	radius *= (0.5 + fabs(v.x));
	glPushMatrix();
	translate_to(pos);
	vector3d scale((0.8+0.5*fabs(v.x)), (0.8+0.5*fabs(v.y)), (0.8+0.5*fabs(v.z)));
	scale *= radius;
	
	if (vdeform != all_ones) { // apply deformation
		float vdmin(1.0);
		UNROLL_3X(scale[i_] *= vdeform[i_]; vdmin = min(vdmin, vdeform[i_]);)
		if (vdmin < 1.0) scale *= pow(1.0/vdmin, 1.0/3.0);
	}
	scale_by(scale);
	glRotatef(360.0*(v.x - v.y), v.x, v.y, (v.z+0.01));
	set_shadowed_color((charred ? BLACK : YELLOW), pos, is_shadowed);
	draw_unit_sphere(ndiv, 0);
	set_shadowed_color((charred ? DK_GRAY : BLOOD_C), pos, is_shadowed);
	glTranslatef(0.1*(v.x-v.y), 0.1*(v.y-v.z), 0.1*(v.x-v.z));
	glRotatef(360.0*(v.z - v.x), v.y, v.z, (v.x+0.01));
	draw_unit_sphere(ndiv, 0);
	glPopMatrix();
}


void draw_grenade(point const &pos, vector3d const &orient, float radius, int ndiv, int time, bool is_shadowed, bool in_ammo, bool is_cgrenade) {

	assert(quadric);
	glPushMatrix();
	translate_to(pos);
	uniform_scale(radius);
	glPushMatrix();
	if (!is_cgrenade) glScalef(0.8, 0.8, 1.2); // rotate also?
	set_color(BLACK);
	draw_unit_sphere(ndiv, 0);
	glPopMatrix();

	float const stime(1.0 - float(time)/float(object_types[is_cgrenade ? CGRENADE : GRENADE].lifetime)), sval(0.2 + 0.8*stime);
	vector3d const vr((orient.x == 0.0 && orient.y == 0.0) ? vector3d(1.0, 0.0, 0.0) : vector3d(orient.x, orient.y, 0.0));
	vector3d vd(plus_z);
	rotate_vector3d_norm(vr, -0.25*PI, vd);
	rotate_about(45.0, vr);
	glTranslatef(0.0, 0.0, 0.7);
	glDisable(GL_CULL_FACE);
	gluCylinder(quadric, 0.3, 0.3, 0.5, max(3, ndiv/2), 1);
	set_color(is_shadowed ? DK_GRAY : GRAY);
	glTranslatef(0.0, 0.0, 0.3);
	gluCylinder(quadric, 0.05, 0.05, sval, max(3, ndiv/4), 1); // fuse
	glPopMatrix();

	if (!animate2) return;
	point const spos(pos + vd*((1.0 + sval)*radius));
	colorRGBA scolor;
	blend_color(scolor, YELLOW, ORANGE, rand_uniform(0.3, 0.7), 1);
	float const size(radius*rand_uniform(0.5, 0.7));
	sparks.push_back(spark_t(spos, scolor, size));
	if (!in_ammo && (rand()&15) == 0) gen_particles(spos, 1, 0.5, 1);
}


void draw_star(point const &pos, vector3d const &orient, vector3d const &init_dir, float radius, float angle, int rotate) { // not all variables used
	
	radius *= 2.0;
	glPushMatrix();
	translate_to(pos);
	uniform_scale(radius);
	up_norm.do_glNormal();

	if (rotate) {
		rotate_by_vector(init_dir, -90.0);
		if (angle != 0.0) rotate_about(angle, orient);
	}
	set_lighted_sides(2);
	glBegin(GL_TRIANGLES);

	for (int i = N_STAR_POINTS-1; i >= 0; --i) {
		int ii((i == 0) ? (N_STAR_POINTS<<1)-1 : (i<<1)-1);
		star_pts[ii].do_glVertex();
		ii = (i << 1);
		star_pts[ii].do_glVertex();
		++ii;
		star_pts[ii].do_glVertex();
	}
	glEnd();
	set_lighted_sides(1);
	glPopMatrix();
}


void draw_shell_casing(point const &pos, vector3d const &orient, vector3d const &init_dir, float radius,
					   float angle, float cd_scale, bool is_shadowed, unsigned char type)
{
	float const point_size(cd_scale/distance_to_camera(pos));
	int const ndiv(max(3, min(N_SPHERE_DIV/2, int(point_size))));
	//glDepthMask(1);
	glPushMatrix();
	translate_to(pos);
	glRotatef(TO_DEG*init_dir.x, 0.0, 0.0, 1.0);
	//rotate_by_vector(init_dir, 0.0);
	rotate_about(angle, orient);
	uniform_scale(radius);
	set_lighted_sides(2);

	if (type == 0) { // M16 shell casing
		gluCylinder(quadric, 1.0, 1.0, 4.0, ndiv, 1);
		if (point_size > 1.0) gluDisk(quadric, 0, 1.0, ndiv, 1);
	}
	else if (type == 1) { // shotgun shell casing
		set_shadowed_color(RED, pos, is_shadowed);
		glTranslatef(0.0, 0.0, -2.0);
		gluCylinder(quadric, 1.2, 1.2, 4.8, ndiv, 1);
		set_shadowed_color(GOLD, pos, is_shadowed);
		glTranslatef(0.0, 0.0, -0.8);
		gluCylinder(quadric, 1.28, 1.28, 1.6, ndiv, 1);
		if (point_size > 1.0) gluDisk(quadric, 0, 1.28, ndiv, 1);
	}
	else {
		assert(0);
	}
	set_lighted_sides(1);
	glPopMatrix();
	//glDepthMask(0);
}


colorRGBA get_glowing_obj_color(point const &pos, int time, int lifetime, float &stime, bool shrapnel_cscale, bool fade) {

	stime = ((float)time)/((float)lifetime)*(shrapnel_cscale ? 1.0 : 0.3);
	if (is_underwater(pos)) stime *= 5.0;
	stime = min(1.0f, 5.0f*stime);

	if (fade) {
		return colorRGBA(1.0, min(1.0, (2.0 - 2.0*stime)), max(0.0f, (1.0f - 1.5f*stime)), min(1.0, (4.0 - 4.0*stime)));
	}
	else {
		return colorRGBA((1.0 - 0.9*stime), max(0.0f, (0.9f - 2.0f*stime)), max(0.0f, (0.6f - 4.0f*stime)), 1.0);
	}
}


void set_glow_color(dwobject const &obj, bool shrapnel_cscale) {

	float stime;
	colorRGBA const color(get_glowing_obj_color(obj.pos, obj.time, object_types[obj.type].lifetime, stime, shrapnel_cscale, ((obj.flags & TYPE_FLAG) != 0)));
	(shrapnel_cscale ? color*CLIP_TO_01(1.0f - stime) : color).do_glColor();
}


void draw_rotated_triangle(point const &pos, vector3d const &o, float radius, float angle, float tscale) {

	/*
	tXX  + c	tXY + sZ	tXZ - sY	0
	tXY - sZ	tYY + c		tYZ + sX	0
	tXZ + sY	tYZ - sX	tZZ + c		0
	0			0			0			1
		c = cos (theta), s = sin (theta), t = 1-cos (theta), and <X,Y,Z>
	x [-r, 0, 0, 1]^-1 => [tX^2 + c,  tXY + sZ, tXZ  - sY, 1]
	x [ 0, 0, q, 1]^-1 => [tXZ  + sY, tYZ - sX, tZ^2 + c,  1]
	*/
	angle *= TO_RADIANS;
	float const r(1.5*radius), q(3.0*radius); // o must be normalized
	float const c(cos(angle)), s(sin(angle)), t(1.0 - c);
	point const p1(r*(t*o.x*o.x + c),     r*(t*o.x*o.y - s*o.z), r*(t*o.x*o.z - s*o.y));
	point const p2(q*(t*o.x*o.z + s*o.y), q*(t*o.y*o.z - s*o.x), q*(t*o.z*o.z + c));
	vector3d const norm(cross_product(p2, p1).get_norm());
	norm.do_glNormal();
	float const ts(123.456*radius), tt(654.321*radius);
	if (tscale != 0.0) glTexCoord2f(ts, tt);
	(pos + p1).do_glVertex();
	if (tscale != 0.0) glTexCoord2f(ts+2*tscale*radius, tt);
	(pos - p1).do_glVertex();
	if (tscale != 0.0) glTexCoord2f(ts, tt+2*tscale*radius);
	(pos + p2).do_glVertex();
}


void draw_shrapnel(dwobject const &obj, float radius, bool is_shadowed) {

	set_glow_color(obj, 1);
	draw_rotated_triangle(obj.pos, obj.orientation, radius, obj.angle, 0.0);
}


void draw_particle(dwobject const &obj, float radius) {

	set_glow_color(obj, 0);
	draw_billboard(obj.pos, get_camera_pos(), up_vector, 1.2*radius, 1.2*radius);
}


void draw_shadow_volume(point const &pos, point const &lpos, float radius, int &inverts) {

	// test for camera inside of cylinder and invert stencil
	vector3d v1(pos - lpos), v2(pos - get_camera_pos());
	float const dotp(dot_product(v1, v2)), val(v1.mag_sq()), length2(sqrt(val));
	if (dotp < 0.0 && (v2 - v1*(dotp/val)).mag_sq() < radius*radius) ++inverts;
	v1 /= length2;
	float const length((zmin - pos.z)/v1.z + radius), radius2(radius*((length + length2)/length2));
	draw_trunc_cone(pos, v1, length, (radius + SMALL_NUMBER), (radius2 + SMALL_NUMBER), 0);
}


// fast and good quality but has other problems:
// 1. slow for many lights (especially double pass mode)
// 2. camera shadow near clip (single pass mode)
// 3. double shadows cancel (single pass mode)
// 4. back faces of objects are double shadowed
// 5. somewhat incorrect for multiple colored lights
int draw_shadowed_objects(int light) {

	int inverts(0);
	point lpos;
	int const shadow_bit(1 << light);
	if (!get_light_pos(lpos, light)) return 0;

	for (int i = 0; i < num_groups; ++i) {
		obj_group const &objg(obj_groups[i]);
		if (!objg.temperature_ok() || !objg.large_radius()) continue;
		float const radius(object_types[objg.type].radius);

		for (unsigned j = 0; j < objg.end_id; ++j) {
			dwobject const &obj(objg.get_obj(j));
			if (obj.disabled()) continue;
			if ((obj.flags & (CAMERA_VIEW | SHADOWED)) || !(obj.shadow & shadow_bit)) continue;
			draw_shadow_volume(obj.pos, lpos, radius, inverts);
		} // for j
	} // for i
	if (display_mode & 0x0200) d_part_sys.add_stencil_shadows(lpos, inverts);
	// loop through cylinders of tree now...or maybe not
	return inverts;
}


void set_specular(float specularity, float shininess) {

	static float last_shiny(-1.0), last_spec(-1.0);
	if (is_cloudy && world_mode != WMODE_UNIVERSE) specularity *= 0.5;

	if (specularity != last_spec) { // This materialfv stuff seems to take some time, so only set if changed since last call
		float mat_specular[]  = {specularity, specularity, specularity, 1.0};
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mat_specular);
		last_spec = specularity;
	}
	if (shininess != last_shiny) {
		float mat_shininess[] = {shininess};
		glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
		last_shiny = shininess;
    }
}


void get_enabled_lights() {

	float a[4], d[4], lval[4];
	for (unsigned j = 0; j < 4; ++j) cur_ambient[j] = ((j == 3) ? 1.0 : 0.0); // reset value
	unsigned ncomp(0);
	enabled_lights.clear();

	for (unsigned i = 0; i < 8; ++i) { // max of 8 lights (GL_LIGHT0 - GL_LIGHT7): sun, moon, lightning
		int const light(GL_LIGHT0 + i); // should be sequential

		if (glIsEnabled(light)) {
			float atten(1.0);
			glGetLightfv(light, GL_AMBIENT, a);
			glGetLightfv(light, GL_DIFFUSE, d);
			glGetLightfv(light, GL_POSITION, lval);
			if (lval[3] != 0.0) glGetLightfv(light, GL_CONSTANT_ATTENUATION, &atten); // point light source only
			assert(atten > 0.0);
			colorRGBA const lcolor(colorRGBA(d[0]/atten, d[1]/atten, d[2]/atten, d[3]));
			enabled_lights.push_back(light_source(0.0, gl_light_positions[i], lcolor, 0, plus_z, 1.0, 0.0, i));
			UNROLL_3X(cur_ambient[i_] += a[i_]/atten;)
			++ncomp;
			//cout << "A: "; cur_ambient.print(); cout << endl;
			//cout << "D: "; lcolor.print(); cout << endl;
		}
	}
	if (ncomp > 0) {
		cur_ambient      *= (0.5 + 0.5/ncomp); // only really valid for sun and moon
		cur_ambient.alpha = 1.0;
	}
}


void set_shadowed_state(unsigned char shadowed) {

	int sval(0);
	
	for (unsigned i = 0; i < enabled_lights.size(); ++i) {
		if (shadowed & (1 << i)) sval |= (1 << enabled_lights[i].get_light_id());
	}
	add_attrib_float(0, float(sval));
}


void set_dlights_booleans(bool enable, int shader_type) {

	set_bool_shader_prefix("has_dir_lights",  has_dir_lights, shader_type);
	set_bool_shader_prefix("enable_dlights",  (enable && dl_tid > 0 && !dl_sources.empty()), shader_type);
}


// texture units used: 0: object texture, 1: smoke texture
colorRGBA setup_smoke_shaders(float min_alpha, int use_texgen, bool keep_alpha, bool indir_lighting,
	bool direct_lighting, bool dlights, bool smoke_en, bool has_lt_atten)
{
	bool const smoke_enabled(smoke_en && smoke_exists && smoke_tid > 0);
	set_int_shader_prefix ("use_texgen",      use_texgen,      0); // VS
	set_bool_shader_prefix("keep_alpha",      keep_alpha,      1); // FS
	set_bool_shader_prefix("indir_lighting",  indir_lighting,  1); // FS
	set_bool_shader_prefix("direct_lighting", direct_lighting, 1); // FS
	set_bool_shader_prefix("do_lt_atten",     has_lt_atten,    1); // FS
	
	for (unsigned i = 0; i < 2; ++i) {
		// Note: dynamic_smoke_shadows applies to light0 only
		// Note: dynamic_smoke_shadows still uses the visible smoke bbox, so if you can't see smoke it won't cast a shadow
		set_bool_shader_prefix("dynamic_smoke_shadows", DYNAMIC_SMOKE_SHADOWS, i); // VS/FS
		set_bool_shader_prefix("smoke_enabled",         smoke_enabled,         i); // VS/FS
	}
	set_dlights_booleans(dlights, 1); // FS
	setup_enabled_lights(8);
	set_shader_prefix("#define USE_GOOD_SPECULAR", 1); // FS
	unsigned const p(set_shader_prog("fog.part+texture_gen.part+line_clip.part*+no_lt_texgen_smoke",
		                             "fresnel.part*+linear_fog.part+ads_lighting.part*+dynamic_lighting.part*+line_clip.part*+textured_with_smoke"));
	setup_scene_bounds(p);
	setup_fog_scale(p); // fog scale for the case where smoke is disabled
	if (dlights && dl_tid > 0) setup_dlight_textures(p);
	unsigned const ix(register_attrib_name(p, "shadow_val"));
	assert(ix == 0); // only one attribute
	add_attrib_float(ix, 0.0); // default is all unshadowed

	if (use_texgen == 2) {
		unsigned const tex0_s_ix(register_attrib_name(p, "tex0_s"));
		unsigned const tex0_t_ix(register_attrib_name(p, "tex0_t"));
		assert(tex0_s_ix == 1 && tex0_t_ix == 2);
	}
	if (smoke_en && smoke_tid) {
		set_multitex(1);
		bind_3d_texture(smoke_tid);
	}
	add_uniform_int(p, "smoke_tex", 1);
	set_multitex(0);
	add_uniform_int(p, "tex0", 0);
	add_uniform_float(p, "min_alpha", min_alpha);
	add_uniform_float_array(p, "smoke_bb", &cur_smoke_bb.d[0][0], 6);
	add_uniform_float(p, "step_delta", HALF_DXY);
	//return change_fog_color(GRAY);

	// setup fog
	colorRGBA old_fog_color;
	glGetFloatv(GL_FOG_COLOR, (float *)&old_fog_color);
	if (smoke_enabled) glFogfv(GL_FOG_COLOR, (float *)&GRAY); // for smoke
	return old_fog_color;
}


void end_smoke_shaders(colorRGBA const &orig_fog_color) {

	unset_shader_prog();
	disable_multitex_a();
	glFogfv(GL_FOG_COLOR, (float *)&orig_fog_color); // reset to original value
}


void setup_object_render_data() {

	RESET_TIME;
	bool const TIMETEST(0);
	get_enabled_lights(); // don't call twice per frame - can have problems with lightning
	init_subdiv_lighting();
	init_draw_stats();
	if (TIMETEST) {PRINT_TIME("Init");}
	distribute_smoke();
	upload_smoke_3d_texture();
	if (TIMETEST) {PRINT_TIME("Distribute Smoke");}
	add_dynamic_lights();
	if (TIMETEST) {PRINT_TIME("Add Dlights");}
	upload_dlights_textures();
	if (TIMETEST) {PRINT_TIME("Dlights Textures");}
	add_coll_shadow_objs();
	if (TIMETEST) {PRINT_TIME("Add Shadows");}
	get_occluders();
	if (TIMETEST) {PRINT_TIME("Get Occluders");}
}


// should always have draw_solid enabled on the first call for each frame
void draw_coll_surfaces(bool draw_solid, bool draw_trans) {

	RESET_TIME;
	assert(draw_solid || draw_trans);
	static vector<pair<float, int> > draw_last;
	if (coll_objects.empty() || world_mode != WMODE_GROUND) return;
	if (!draw_solid && draw_last.empty() && (!smoke_exists || portals.empty())) return; // nothing transparent to draw
	set_lighted_sides(2);
	set_fill_mode();
	gluQuadricTexture(quadric, GL_FALSE);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glDisable(GL_LIGHTING); // custom lighting calculations from this point on
	set_color_a(BLACK);
	set_specular(0.0, 1.0);
	bool const has_lt_atten(draw_trans && !draw_solid);
	colorRGBA const orig_fog_color(setup_smoke_shaders(0.0, (USE_ATTR_TEXGEN ? 2 : 1), 0, 1, 1, 1, 1, has_lt_atten)); // Note: enable direct_lighting if processing sun/moon shadows here
	int last_tid(-1), last_group_id(-1), last_pri_dim(-1);
	
	if (draw_solid) {
		draw_last.resize(0);
		
		if (have_drawn_cobj) {
			for (unsigned i = 0; i < coll_objects.size(); ++i) {
				if (coll_objects[i].no_draw()) continue;

				if (coll_objects[i].is_semi_trans()) {
					float const neg_dist_sq(-distance_to_camera_sq(coll_objects[i].get_center_pt()));
					draw_last.push_back(make_pair(neg_dist_sq, i));
				}
				else {
					coll_objects[i].draw_cobj(i, last_tid, last_group_id, last_pri_dim);
				}
			}
		}
	}
	if (draw_trans) { // called second
		if (smoke_exists) {
			for (unsigned i = 0; i < portals.size(); ++i) {
				if (!portals[i].is_visible()) continue;
				float const neg_dist_sq(-distance_to_camera_sq(portals[i].get_center_pt()));
				draw_last.push_back(make_pair(neg_dist_sq, -(int)(i+1)));
			}
		}
		sort(draw_last.begin(), draw_last.end()); // sort back to front
		enable_blend();
		int ulocs[3] = {0};
		float last_light_atten(-1.0), last_refract_ix(0.0); // set to invalid values to start

		if (has_lt_atten) {
			ulocs[0] = get_uniform_loc(0, "light_atten");
			ulocs[1] = get_uniform_loc(0, "cube_bb"    );
			ulocs[2] = get_uniform_loc(0, "refract_ix" );
		}
		for (unsigned i = 0; i < draw_last.size(); ++i) {
			int const ix(draw_last[i].second);

			if (ix < 0) { // portal
				if (has_lt_atten && last_light_atten != 0.0) {
					set_uniform_float(ulocs[0], 0.0);
					last_light_atten = 0.0;
				}
				if (has_lt_atten && last_refract_ix != 1.0) {
					set_uniform_float(ulocs[2], 1.0);
					last_refract_ix = 1.0;
				}
				unsigned const pix(-(ix+1));
				assert(pix < portals.size());
				portals[pix].draw();
			}
			else { // cobj
				assert((unsigned)ix < coll_objects.size());
				coll_obj &c(coll_objects[ix]);
				
				if (has_lt_atten) { // we only support cubes for now
					float const light_atten((c.type == COLL_CUBE) ? c.cp.light_atten : 0.0);

					if (light_atten != last_light_atten) {
						set_uniform_float(ulocs[0], light_atten);
						last_light_atten = light_atten;
					}
					if (c.cp.refract_ix != last_refract_ix) {
						set_uniform_float(ulocs[2], c.cp.refract_ix);
						last_refract_ix = c.cp.refract_ix;
					}
					if (light_atten > 0.0) set_uniform_float_array(ulocs[1], (float const *)c.d, 6);
				}
				c.draw_cobj(ix, last_tid, last_group_id, last_pri_dim);
			}
		}
		disable_blend();
		draw_last.resize(0);
	}
	if (last_group_id >= 0) glEnd(); // end group
	end_smoke_shaders(orig_fog_color);
	glEnable(GL_LIGHTING);
	disable_textures_texgen();
	set_lighted_sides(1);
	set_specular(0.0, 1.0);

	if (draw_solid) {
		//PRINT_TIME("Final Draw");
		show_draw_stats();
	}
}


bool portal::is_visible() const {

	point center;
	float rad;
	polygon_bounding_sphere(pts, 4, 0.0, center, rad);
	return sphere_in_camera_view(center, rad, 2);
}


void portal::draw() const {

	float const scale[2] = {0.0, 0.0}, xlate[2] = {0.0, 0.0};
	select_texture(WHITE_TEX, 0);
	setup_polygon_texgen(plus_z, scale, xlate); // doesn't matter as long as it's set to something
	ALPHA0.do_glColor();
	//WHITE.do_glColor();
	glBegin(GL_QUADS);

	for (unsigned i = 0; i < 4; ++i) {
		pts[i].do_glVertex();
	}
	glEnd();
};


void draw_stars(float alpha) {

	assert(num_stars <= stars.size());
	if (alpha <= 0.0) return;
	colorRGBA color(BLACK), bkg;
	UNROLL_3X(bkg[i_] = (1.0 - alpha)*bkg_color[i_];)
	glPushMatrix();
	if (camera_mode == 1) translate_to(surface_pos);
	up_norm.do_glNormal();
	set_color(BLACK);
	enable_blend();
	glPointSize(2.0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glBegin(GL_POINTS);
	
	for (unsigned i = 0; i < num_stars; ++i) {
		if ((rand()%400) == 0) continue; // flicker out

		for (unsigned j = 0; j < 3; ++j) {
			float const c(stars[i].color[j]*stars[i].intensity);
			color[j] = ((alpha >= 1.0) ? c : (alpha*c + bkg[j]));
		}
		color.do_glColor();
		stars[i].pos.do_glVertex();
	}
	glEnd();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glPointSize(1.0);
	disable_blend();
	glPopMatrix();
}


void draw_sun() {

	if (!have_sun) return;
	point const pos(get_sun_pos());

	if (sphere_in_camera_view(pos, sun_radius, 1)) {
		select_texture(SUN_TEX);
		glDisable(GL_LIGHTING);
		colorRGBA color(SUN_C);
		apply_red_sky(color);
		color.do_glColor();
		draw_subdiv_sphere(pos, sun_radius, N_SPHERE_DIV, 1, 0);
		glEnable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);
	}
}


void draw_moon() {

	if (show_fog) return; // don't draw when there is fog
	point const pos(get_moon_pos());
	if (!sphere_in_camera_view(pos, moon_radius, 1)) return;
	set_color(WHITE);
	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHT1);
	float const ambient[4] = {0.05, 0.05, 0.05, 1.0}, diffuse[4] = {1.0, 1.0, 1.0, 1.0};

	if (have_sun) {
		set_gl_light_pos(GL_LIGHT4, get_sun_pos(), 0.0);
		set_colors_and_enable_light(GL_LIGHT4, ambient, diffuse);
	}
	select_texture(MOON_TEX);
	draw_subdiv_sphere(pos, moon_radius, N_SPHERE_DIV, 1, 0);
	glDisable(GL_TEXTURE_2D);
	if (light_factor < 0.6) glEnable(GL_LIGHT1); // moon
	if (light_factor > 0.4) glEnable(GL_LIGHT0); // sun
	glDisable(GL_LIGHT4);

	if (light_factor >= 0.4) { // fade moon into background color when the sun comes up
		colorRGBA color = bkg_color;
		color.alpha     = 5.0*(light_factor - 0.4);
		glDisable(GL_LIGHTING);
		enable_blend();
		color.do_glColor();
		draw_subdiv_sphere(pos, 1.2*moon_radius, N_SPHERE_DIV, 0, 0);
		glEnable(GL_LIGHTING);
		disable_blend();
	}
}


// for some reason the texture is backwards, so we mirrored the image of the earth
void draw_earth() {

	if (show_fog) return; // don't draw when there is fog
	point pos(mesh_origin + earth_pos);
	if (camera_mode == 1) pos += surface_pos;
	static float rot_angle(0.0);

	if (quadric != 0 && sphere_in_camera_view(pos, earth_radius, 1)) {
		set_fill_mode();
		select_texture(EARTH_TEX);
		set_color(WHITE);
		glPushMatrix();
		translate_to(pos);
		glRotatef(67.0, 0.6, 0.8, 0.0);
		glRotatef(rot_angle, 0.0, 0.0, 1.0);
		glRotatef(180.0, 1.0, 0.0, 0.0);
		draw_sphere_dlist(all_zeros, earth_radius, N_SPHERE_DIV, 1);
		glPopMatrix();
		glDisable(GL_TEXTURE_2D);
	}
	rot_angle += 0.2*fticks;
}


void draw_stationary_earth(float radius) {

	set_fill_mode();
	select_texture(EARTH_TEX);
	set_color(WHITE);
	draw_subdiv_sphere(all_zeros, radius, N_SPHERE_DIV, 1, 0);
	glDisable(GL_TEXTURE_2D);
}


void apply_red_sky(colorRGBA &color) {

	if (light_factor > 0.45 && light_factor < 0.55) { // red sky at night/morning
		float const redness(1.0 - 20.0*fabs(light_factor - 0.5));
		color.red   = min(1.0f, (1.0f + 0.8f*redness)*color.red);
		color.green = max(0.0f, (1.0f - 0.2f*redness)*color.green);
		color.blue  = max(0.0f, (1.0f - 0.5f*redness)*color.blue);
	}
}


colorRGBA get_cloud_color() {

	colorRGBA color(brightness, brightness, brightness, atmosphere);
	apply_red_sky(color);
	return color;
}


float get_cloud_density(point const &pt, vector3d const &dir) { // optimize?

	if (atmosphere == 0.0) return 0.0;
	point lsint;
	if (!line_sphere_int(dir*-1.0, pt, cur_spo.center, cur_spo.radius, lsint, 0)) return 0.0; // shouldn't get here?
	vector3d const vdir(lsint - cur_spo.center);
	return atmosphere*get_texture_component(CLOUD_TEX, (vdir.x/cur_spo.radius + cur_spo.dx), (vdir.y/cur_spo.radius + cur_spo.dy), 3); // cloud alpha
}


void draw_sky(int order) {

	if (atmosphere < 0.01) {
		cloud_volumes.clear();
		return; // no atmosphere
	}
	set_specular(0.0, 1.0);
	float radius(0.55*(FAR_CLIP+X_SCENE_SIZE));
	point center((camera_mode == 1) ? surface_pos : mesh_origin);
	center.z -= 0.727*radius;
	if ((distance_to_camera(center) > radius) != order) return;
	colorRGBA const cloud_color(get_cloud_color());

	static float sky_rot_xy[2] = {0.0, 0.0}; // x, y
	float const wmag(sqrt(wind.x*wind.x + wind.y*wind.y));

	if (wmag > TOLERANCE) {
		for (unsigned d = 0; d < 2; ++d) {
			sky_rot_xy[d] += fticks*CLOUD_WIND_SPEED*(wmag + 0.5*WIND_ADJUST)*wind[d]/wmag;
		}
	}
	cur_spo = sky_pos_orient(center, radius, sky_rot_xy[0], sky_rot_xy[1]);
	int const light(GL_LIGHT4);
	set_fill_mode();
	enable_blend();

	if (have_sun && light_factor > 0.4) { // sun lighting of clouds
		float diffuse[4], ambient[4];
		point lpos(get_sun_pos()), lsint;
		vector3d const sun_v((get_camera_pos() - lpos).get_norm());
		if (line_sphere_int(sun_v, lpos, center, radius, lsint, 1)) lpos = lsint;
		
		for (unsigned i = 0; i < 4; ++i) { // even alpha?
			diffuse[i] = 1.0*sun_color[i];
			ambient[i] = 0.5*sun_color[i];
		}
		set_gl_light_pos(light, lpos, 1.0); // w - point light source
		set_colors_and_enable_light(light, ambient, diffuse);
		glLightf(light, GL_CONSTANT_ATTENUATION,  0.0);
		glLightf(light, GL_LINEAR_ATTENUATION,    0.01);
		glLightf(light, GL_QUADRATIC_ATTENUATION, 0.01);
	}
	if (have_sun && light_factor > 0.4) { // draw horizon
		glDisable(GL_LIGHTING);
		colorRGBA horizon_color;
		float const blend_val(atmosphere*CLIP_TO_01(10.0f*(light_factor - 0.4f)));
		blend_color(horizon_color, WHITE, ALPHA0, blend_val, 1);
		horizon_color.alpha *= 0.5;
		apply_red_sky(horizon_color);
		horizon_color.do_glColor();
		select_texture(GRADIENT_TEX);
		draw_sphere_dlist(center, 1.05*radius, N_SPHERE_DIV, 1);
		glEnable(GL_LIGHTING);
	}
	select_texture(CLOUD_TEX);

	// change S and T parameters to map sky texture into the x/y plane with translation based on wind/rot
	setup_texgen(1.0/radius, 1.0/radius, (sky_rot_xy[0] - center.x/radius), (sky_rot_xy[1] - center.y/radius)); // GL_EYE_LINEAR
	set_color(cloud_color); // disable lighting (BLACK)?
	//draw_sphere_at(center, radius, (3*N_SPHERE_DIV)/2);
	draw_subdiv_sphere(center, radius, (3*N_SPHERE_DIV)/2, zero_vector, NULL, 0, 1);
	disable_textures_texgen(); // reset S and T parameters
	disable_blend();

	if (display_mode & 0x40) {
		if (cloud_volumes.empty()) gen_cloud_volumes();
		draw_cloud_volumes(); // detailed clouds
	}
	glDisable(light);
}


void draw_stationary_sky(float radius, float density) {

	colorRGBA color(WHITE);
	color.alpha = density;
	set_fill_mode();
	enable_blend();
	select_texture(CLOUD_TEX);
	set_color(color);
	draw_subdiv_sphere(all_zeros, radius, N_SPHERE_DIV, 1, 0);
	glDisable(GL_TEXTURE_2D);
	disable_blend();
}


void compute_brightness() {

	brightness = 0.8 + 0.2*light_factor;
	if (!have_sun) brightness *= 0.25;
	if (is_cloudy) brightness *= 0.5;
	
	if (light_pos.z < zmin) {
		brightness *= 0.1;
	}
	else if (light_factor <= 0.4 || light_factor >= 0.6) {
		brightness *= 0.15 + 0.85*light_pos.z/light_pos.mag();
	}
	else {
		float const sun_bright (sun_pos.z /sun_pos.mag() );
		float const moon_bright(moon_pos.z/moon_pos.mag());
		brightness *= 0.15 + 0.85*5.0*((light_factor - 0.4)*sun_bright + (0.6 - light_factor)*moon_bright);
	}
	brightness = CLIP_TO_01(brightness);
}


typedef vector<pair<float, unsigned> > order_vect_t;


template<typename T> void get_draw_order(vector<T> const &objs, order_vect_t &order) {

	point const camera(get_camera_pos());
	
	for (unsigned i = 0; i < objs.size(); ++i) {
		point const pos(objs[i].get_pos());

		if (objs[i].status && sphere_in_camera_view(pos, objs[i].radius, 0)) {
			order.push_back(make_pair(-p2p_dist_sq(pos, camera), i));
		}
	}
	sort(order.begin(), order.end()); // sort back to front
}


void bubble::draw() const {

	assert(status);
	colorRGBA color2(color);
	if (world_mode == WMODE_GROUND) select_liquid_color(color2, pos);
	float const point_dia(NDIV_SCALE*window_width*radius/distance_to_camera(pos));

	if (point_dia < 4.0) {
		obj_pld.add_pt(pos, (get_camera_pos() - pos), color2);
	}
	else {
		set_color(color2);
		int const ndiv(max(4, min(16, int(4.0*sqrt(point_dia)))));
		draw_sphere_dlist(pos, radius, ndiv, 0, 0);
	}
}


void particle_cloud::draw() const {

	assert(status);
	float const scale(get_zoom_scale()*0.016*window_width);
	colorRGBA color(base_color);

	if (is_fire()) {
		color.green *= get_rscale();
	}
	else {
		color *= (0.5*(1.0 - darkness));
	}
	color.alpha *= density;
	float const dist(distance_to_camera(pos));
	int const ndiv(max(4, min(16, int(scale/dist))));

	if (parts.empty()) {
		if (status && sphere_in_camera_view(pos, radius, 0)) {
			draw_part(pos, radius, color);
		}
	}
	else {
		order_vect_t order;
		vector<part> cur_parts(parts);

		for (unsigned i = 0; i < cur_parts.size(); ++i) {
			cur_parts[i].pos     = pos + cur_parts[i].pos*radius;
			cur_parts[i].radius *= radius;
		}
		get_draw_order(cur_parts, order);
		
		for (unsigned j = 0; j < order.size(); ++j) {
			unsigned const i(order[j].second);
			assert(i < cur_parts.size());
			draw_part(cur_parts[i].pos, cur_parts[i].radius, color);
		}
	}
}


void particle_cloud::draw_part(point const &p, float r, colorRGBA c) const {

	if (!is_fire()) { // fire has its own emissive lighting
		int cindex;
		float rad, dist, t;
		point const lpos(get_light_pos());
	
		if (!check_coll_line(p, lpos, cindex, -1, 1, 1)) { // not shadowed (slow, especially for lots of smoke near trees)
			// Note: This can be moved into a shader, but the performance and quality improvement might not be significant
			vector3d const dir((p - get_camera_pos()).get_norm());
			float const dp(dot_product_ptv(dir, p, lpos));
			blend_color(c, WHITE, c, 0.15, 0); // 15% ambient lighting (transmitted/scattered)
			if (dp > 0.0) blend_color(c, WHITE, c, 0.1*dp/p2p_dist(p, lpos), 0); // 10% diffuse lighting (directional)

			if (dp < 0.0 && have_sun && line_intersect_sphere(p, dir, sun_pos, 6*sun_radius, rad, dist, t)) {
				float const mult(1.0 - max(0.0f, (rad - sun_radius)/(5*sun_radius)));
				blend_color(c, SUN_C, c, 0.75*mult, 0); // 75% direct sun lighting
			}
		}
		get_indir_light(c, WHITE, p, 0, 1, NULL, NULL); // could move outside of the parts loop if too slow
	}
	c.do_glColor();
	// Note: Can disable smoke volume integration for close smoke, but very close smoke (< 1 grid unit) is infrequent
	draw_billboard(p, get_camera_pos(), up_vector, 4.0*r, 4.0*r);
}


void fire::set_fire_color() const {

	float const alpha(rand_uniform(max(0.3, (0.9 + 0.1*heat)), min(0.9, (0.8 + 0.2*heat))));
	colorRGBA const color(1.0, 0.4*heat, max(0.0f, 1.2f*(heat-1.0f)), alpha);
	color.do_glColor();
}


void fire::draw() const {

	assert(status);
	point const pos2(pos + point(0.0, 0.0, 2.0*radius));
	WHITE.do_glColor();
	draw_animated_billboard(pos2, 4.0*radius, (time&15)/16.0);
}


void scorch_mark::draw() const {

	assert(status);
	colorRGBA(rgb_val, rgb_val, rgb_val, get_alpha()).do_glColor();
	vector3d const upv(orient.y, orient.z, orient.x); // swap the xyz values to get an orthogonal vector
	point const cur_pos(get_pos());
	draw_billboard(cur_pos, (cur_pos + orient), upv, radius, radius);
}


template<typename T> void draw_objects(vector<T> const &objs) {

	order_vect_t order;
	get_draw_order(objs, order);

	for (unsigned i = 0; i < order.size(); ++i) {
		assert(order[i].second < objs.size());
		objs[order[i].second].draw();
	}
}


void draw_bubbles() {

	if (bubbles.empty()) return;
	glEnable(GL_CULL_FACE);
	enable_blend();
	set_color(WATER_C);
	draw_objects(bubbles);
	obj_pld.draw_and_clear();
	disable_blend();
	glDisable(GL_CULL_FACE);
}


void draw_part_cloud(vector<particle_cloud> const &pc, colorRGBA const color, bool zoomed) {

	enable_flares(color, zoomed); // color will be set per object
	//select_multitex(CLOUD_TEX, 1);
	glAlphaFunc(GL_GREATER, 0.01);
	glEnable(GL_ALPHA_TEST); // makes it faster
	enable_blend();
	glBegin(GL_QUADS);
	draw_objects(pc);
	glEnd();
	disable_blend();
	glDisable(GL_ALPHA_TEST);
	disable_flares();
	//disable_multitex_a();
}


void draw_smoke() {

	if (part_clouds.empty()) return; // Note: just because part_clouds is empty doesn't mean there is any enabled smoke
	set_color(BLACK);
	colorRGBA const orig_fog_color(setup_smoke_shaders(0.01, 0, 1, 0, 0, 0, 1)); // slow when a lot of smoke is up close
	draw_part_cloud(part_clouds, WHITE, 0);
	end_smoke_shaders(orig_fog_color);
}


struct cloud_t {
	unsigned b, e;
	point p;
	float r;
	cloud_t(unsigned b_=0, unsigned e_=0, point const &p_=all_zeros, float r_=0.0)
		: b(b_), e(e_), p(p_), r(r_) {}
};


//http://www.gamedev.net/reference/articles/article2273.asp
void draw_cloud_volumes() {

	if (cloud_volumes.empty()) return;
	if (atmosphere < 0.01)     return; // no atmosphere
	//glFinish(); // testing
	RESET_TIME;

	// WRITE: wind moves clouds

	// light source code
	static bool had_sun(0);
	static float last_sun_rot(0.0);

	if (sun_rot != last_sun_rot || have_sun != had_sun) { // update lighting
		last_sun_rot = sun_rot;
		had_sun      = have_sun;
		point const sun_pos(get_sun_pos());
		bool const calc_sun_light(have_sun && light_factor > 0.4);
		unsigned const num_clouds(cloud_volumes.size());
		int last_src(0);
		vector<cloud_t> clouds;

		if (calc_sun_light) {
			for (unsigned i = 0; i < num_clouds; ++i) {
				particle_cloud &c(cloud_volumes[i]);

				if (i == 0 || c.source != last_src) {
					last_src = c.source;
					if (i > 0) clouds.back().e = i; // end the last cloud
					clouds.push_back(cloud_t(i));   // begin a new cloud
				}
			}
			clouds.back().e = num_clouds; // end the last cloud

			for (unsigned i = 0; i < clouds.size(); ++i) {
				cloud_t &c(clouds[i]);
				for (unsigned j = c.b; j < c.e; ++j) c.p += cloud_volumes[j].pos;
				c.p /= (c.e - c.b);
				for (unsigned j = c.b; j < c.e; ++j) c.r = max(c.r, (p2p_dist(c.p, cloud_volumes[j].pos) + cloud_volumes[j].radius));
			}
		}
		for (unsigned i = 0; i < num_clouds; ++i) {
			particle_cloud &pc(cloud_volumes[i]);
			float light(0.25); // night time sky

			if (calc_sun_light) {
				vector3d const v1(sun_pos - pc.pos);
				float const dist_sq(v1.mag_sq());
				vector3d const v1n(v1/dist_sq);
				light = 1.0; // start off fully lit

				for (unsigned p = 0; p < clouds.size(); ++p) {
					cloud_t &c(clouds[p]);
					float t; // unused

					if (sphere_test_comp(sun_pos, c.p, v1, c.r*c.r, t)) {
						for (unsigned j = c.b; j < c.e; ++j) {
							particle_cloud const &c2(cloud_volumes[j]);
							vector3d const v2(sun_pos, c2.pos);
							if (v2.mag_sq() > dist_sq) continue; // further from the sun
							float const dotp(dot_product(v1, v2));
							float const dsq((dotp > dist_sq) ? p2p_dist_sq(v1, v2) : (v2 - v1n*dotp).mag_sq());
							if (dsq > c2.radius*c2.radius) continue; // no intersection
							float const alpha(2.0*c2.base_color.alpha*c2.density*((c2.radius - sqrt(dsq))/c2.radius));
							light *= 1.0 - CLIP_TO_01(alpha);
						}
					}
				}
				if (light_factor < 0.6) {
					float const blend(5.0*(light_factor - 0.4));
					light = light*blend + 0.25*(1.0 - blend);
				}
			}
			pc.darkness   = 1.0 - 2.0*light;
			pc.base_color = WHITE;
			apply_red_sky(pc.base_color);
		}
		PRINT_TIME("Cloud Lighting");
	}
	glDisable(GL_DEPTH_TEST);
	draw_part_cloud(cloud_volumes, get_cloud_color(), 1);
	glEnable(GL_DEPTH_TEST);
	//glFinish(); // testing
	//PRINT_TIME("Clouds");
}


template<typename T> void draw_billboarded_objs(obj_vector_t<T> const &objs, int tid) {

	order_vect_t order;
	get_draw_order(objs, order);
	if (order.empty()) return;
	colorRGBA const orig_fog_color(setup_smoke_shaders(0.04, 0, 1, 0, 0, 0, 1));
	enable_blend();
	set_color(BLACK);
	glDisable(GL_LIGHTING);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.04);
	select_texture(tid);
	glBegin(GL_QUADS);

	for (unsigned j = 0; j < order.size(); ++j) {
		unsigned const i(order[j].second);
		assert(i < objs.size());
		objs[i].draw();
	}
	glEnd();
	end_smoke_shaders(orig_fog_color);
	glDisable(GL_ALPHA_TEST);
	disable_blend();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
}


void draw_fires() {

	// animated fire textured quad
	//glDisable(GL_DEPTH_TEST);
	draw_billboarded_objs(fires, FIRE_TEX);
}


void draw_scorches() {

	draw_billboarded_objs(scorches, BLUR_CENT_TEX);
}


void add_camera_filter(colorRGBA const &color, unsigned time, int tid, unsigned ix) {

	assert(ix < MAX_CFILTERS);
	if (color.alpha == 0.0) return;
	if (cfilters.size() <= ix) cfilters.resize(ix+1);
	cfilters[ix] = camera_filter(color, time, tid);
}


void camera_filter::draw() {

	bool const tex(tid >= 0 && glIsTexture(tid));
	if (tex) select_texture(tid);
	glBegin(GL_QUADS);
	float const zval(-1.1*perspective_nclip), tan_val(tan(perspective_fovy/TO_DEG));
	float const y(0.5*zval*tan_val), x((y*window_width)/window_height);
	color.do_glColor();
	draw_one_tquad(-x, -y, x, y, zval, tex);
	glEnd();
	if (tex) glDisable(GL_TEXTURE_2D);
}


void draw_camera_filters(vector<camera_filter> &cfs) {

	if (cfs.empty()) return;
	GLboolean lighting(glIsEnabled(GL_LIGHTING));
	if (lighting) glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	enable_blend();

	for (int i = cfs.size()-1; i >= 0; --i) { // apply backwards
		if (cfs[i].time == 0) continue;
		cfs[i].draw();
		if ((int)cfs[i].time <= iticks) cfs[i].time = 0; else cfs[i].time -= iticks;
	}
	disable_blend();
	glEnable(GL_DEPTH_TEST);
	if (lighting) glEnable(GL_LIGHTING);
}


int   spark_t::status = 1;
float spark_t::radius = 0.0;


void spark_t::draw() const {

	c.do_glColor();
	point const camera(get_camera_pos());
	draw_billboard((pos + (camera - pos).get_norm()*0.02), camera, up_vector, s, s);
}


void draw_sparks() {

	if (sparks.empty()) return;
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_LIGHTING);
	enable_blend();
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.01);
	select_texture(BLUR_TEX);
	glBegin(GL_QUADS);
	draw_objects(sparks);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
	glDisable(GL_ALPHA_TEST);
	disable_blend();
	set_fill_mode();
	sparks.clear();
}


void draw_projectile_effects() {

	update_blasts(); // not really an update, but needed for draw_blasts
	draw_blasts();
	draw_lasers();
	draw_sparks();
}


void draw_env_other() {

	if (!enable_fsource) return;
	set_color(BLACK);
	draw_subdiv_sphere(flow_source, 0.05, N_SPHERE_DIV, 0, 0);
}


void mouse_draw_on_ground(int x, int y) {

	swap(x, y); // landscape is rotated by 90 degrees
	int const xscale(window_height), yscale(window_height);
	int const xpos(int((float(x - 0.5*(window_width-window_height))/(float)xscale)*MESH_X_SIZE));
	int const ypos(int(((float)y/(float)yscale)*MESH_Y_SIZE));
	if (point_outside_mesh(xpos, ypos)) return;
	accumulation_matrix[ypos][xpos] += 1000.0;
	add_color_to_landscape_texture(WHITE, get_xval(xpos), get_yval(ypos), 1.0, 0);
}


void draw_splash(float x, float y, float z, float size, colorRGBA color) {

	assert(quadric && size >= 0.0);
	if (size == 0.0 || temperature <= W_FREEZE_POINT && !island) return;
	if (size > 0.1) size = sqrt(10.0*size)/10.0;
	unsigned const num_rings(min(10U, (unsigned)ceil(size)));
	size = min(size, 0.025f);
	float radius(size);
	float const dr(0.5*size);
	point const pos(x, y, z+SMALL_NUMBER);
	unsigned const ndiv(max(3, min(N_CYL_SIDES, int(1000.0*size/max(TOLERANCE, distance_to_camera(pos))))));
	select_liquid_color(color, get_xpos(x), get_ypos(y));
	set_color(color);
	set_fill_mode();
	glPushMatrix();
	translate_to(pos);

	for (unsigned i = 0; i < num_rings; ++i) {
		gluDisk(quadric, (radius - 0.5*dr), radius, ndiv, 1);
		radius += dr;
	}
	glPopMatrix();
}


void draw_text(float x, float y, float z, char const *text, float tsize, bool bitmap_font) {

	//bitmap_font |= ((display_mode & 0x80) != 0);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	if (bitmap_font) {
		glRasterPos3f(x, y, z);
	}
	else {
		up_norm.do_glNormal();
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		glPushMatrix();
		glTranslatef(x, y, z);
		uniform_scale(0.000005*tsize);
	}
	unsigned line_num(0);

	while (*text) {
		if (*text == '\n') { // newline (CR/LF)
			++line_num;

			if (bitmap_font) {
				glRasterPos3f(x, y-(0.5*line_num)/window_height, z);
			}
			else {
				glPopMatrix();
				glPushMatrix();
				glTranslatef(x, y-0.001*line_num*tsize, z);
				uniform_scale(0.000005*tsize);
			}
		}
		else {
			if (bitmap_font) {
				glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *text); // other fonts available
			}
			else {
				glutStrokeCharacter(GLUT_STROKE_ROMAN, *text); // GLUT_STROKE_MONO_ROMAN
			}
		}
		text++;
	}
	if (!bitmap_font) {
		glPopMatrix();
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}


void draw_framerate(float val) {

	char text[32];
	WHITE.do_glColor();
	sprintf(text, "%3.1f", val);
	float const ar(((float)window_width)/((float)window_height));
	draw_text(-0.011*ar, -0.011, -2.0*NEAR_CLIP, text);
}


void draw_compass_and_alt() { // and temperature

	char text[64];
	float const aspect_ratio((float)window_width/(float)window_height);
	static std::string dirs[8] = {"N", "NW", "W", "SW", "S", "SE", "E", "NE"};
	YELLOW.do_glColor();
	sprintf(text, "Loc: (%3.2f, %3.2f, %3.2f)", (camera_origin.x+xoff2*DX_VAL), (camera_origin.y+yoff2*DY_VAL), camera_origin.z);
	draw_text(-0.005*aspect_ratio, -0.01, -0.02, text);
	float const theta(safe_acosf(-cview_dir.x)*TO_DEG);
	int const octant(int(((cview_dir.y > 0) ? (360.0 - theta) : theta)/45.0 + 2.5)%8);
	sprintf(text, "%s", dirs[octant].c_str());
	draw_text(0.005*aspect_ratio, -0.01, -0.02, text);
	sprintf(text, "Temp: %iC", int(temperature));
	draw_text(0.007*aspect_ratio, -0.01, -0.02, text);
}



