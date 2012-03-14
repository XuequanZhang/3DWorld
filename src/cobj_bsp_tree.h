// 3D World Collision Object BSP/KD/Oct Tree
// by Frank Gennari
// 11/15/11
#ifndef _COBJ_BSP_TREE_H_
#define _COBJ_BSP_TREE_H_

#include "physics_objects.h"


class cobj_tree_base {

protected:
	struct tree_node : public cube_t { // size = 36
		unsigned start, end; // index into cixs for leaves
		unsigned next_node_id;

		tree_node(unsigned s=0, unsigned e=0) : start(s), end(e), next_node_id(0) {
			UNROLL_3X(d[i_][0] = d[i_][1] = 0.0;)
		}
		unsigned get_split_dim(float &max_sz, float &sval, unsigned skip_dims) const;
	};

	vector<tree_node> nodes;
	unsigned max_depth, max_leaf_count, num_leaf_nodes;

	inline void register_leaf(unsigned num) {
		++num_leaf_nodes;
		max_leaf_count = max(max_leaf_count, num);
	}
	bool check_for_leaf(unsigned num, unsigned skip_dims);
	unsigned get_conservative_num_nodes(unsigned num) const {return (4*num/3 + 8);}

	struct node_ix_mgr {
		point const p1, p2;
		vector3d dinv;
		vector<tree_node> const &nodes;

		node_ix_mgr(vector<tree_node> const &nodes_, point const &p1_, point const &p2_)
			: nodes(nodes_), p1(p1_), p2(p2_), dinv(p2 - p1) {dinv.invert(0, 1);}
		bool check_node(unsigned &nix) const;
	};

public:
	bool is_empty() const {return nodes.empty();}
	void clear() {nodes.resize(0);}
};


class cobj_tree_tquads_t : public cobj_tree_base {

	vector<coll_tquad> tquads, temp_bins[3];

	void calc_node_bbox(tree_node &n) const;
	void build_tree(unsigned nix, unsigned skip_dims, unsigned depth);

public:
	void clear() {
		cobj_tree_base::clear();
		tquads.clear(); // reserve(0)?
	}
	vector<coll_tquad> &get_tquads_ref() {return tquads;}
	void build_tree_top(bool verbose);
	void add_cobjs(coll_obj_group const &cobjs, bool verbose);
	void add_polygons(vector<polygon_t> const &polygons, bool verbose);
	bool check_coll_line(point const &p1, point const &p2, point &cpos, vector3d &cnorm, colorRGBA *color, int *cindex, int ignore_cobj, bool exact) const;

	bool check_coll_line(point const &p1, point const &p2, point &cpos, vector3d &cnorm, int &cindex, int ignore_cobj, bool exact) const {
		cindex = -1;
		return check_coll_line(p1, p2, cpos, cnorm, NULL, &cindex, ignore_cobj, exact);
	}
	bool check_coll_line(point const &p1, point const &p2, point &cpos, vector3d &cnorm, colorRGBA &color, bool exact) const {
		return check_coll_line(p1, p2, cpos, cnorm, &color, NULL, 0, exact);
	}
};


class cobj_bvh_tree : public cobj_tree_base {

	coll_obj_group const &cobjs;
	vector<unsigned> cixs;
	bool is_static, is_dynamic, occluders_only, moving_only, cubes_only;

	struct per_thread_data {
		vector<unsigned> temp_bins[3];
		unsigned start_nix, end_nix, cur_nix;
		per_thread_data(unsigned start, unsigned end) : start_nix(start), end_nix(end), cur_nix(start) {}
		unsigned get_next_node_ix() const {return cur_nix;}
		void increment_node_ix() {assert(cur_nix >= start_nix && cur_nix < end_nix); cur_nix++;}
	};

	void add_cobj(unsigned ix) {if (obj_ok(cobjs[ix])) cixs.push_back(ix);}
	coll_obj const &get_cobj(unsigned ix) const {return cobjs[cixs[ix]];}
	bool create_cixs();
	void calc_node_bbox(tree_node &n) const;
	void build_tree_top_level_omp();
	void build_tree(unsigned nix, unsigned skip_dims, unsigned depth, per_thread_data &ptd);

	bool obj_ok(coll_obj const &c) const {
		return (((is_static && c.status == COLL_STATIC) || (is_dynamic && c.status == COLL_DYNAMIC) || (!is_static && !is_dynamic)) &&
			(!occluders_only || c.is_occluder()) && (!moving_only || c.maybe_is_moving()) && !c.cp.no_coll && (!cubes_only || c.type == COLL_CUBE));
	}

public:
	cobj_bvh_tree(coll_obj_group const &cobjs_, bool s, bool d, bool o, bool m, bool c=0)
		: cobjs(cobjs_), is_static(s), is_dynamic(d), occluders_only(o), moving_only(m), cubes_only(c) {}

	void clear() {
		cobj_tree_base::clear();
		cixs.resize(0);
	}
	void add_cobjs(bool verbose);
	bool check_coll_line(point const &p1, point const &p2, point &cpos, vector3d &cnorm, int &cindex, int ignore_cobj,
		bool exact, int test_alpha, bool skip_non_drawn) const;
	void get_intersecting_cobjs(cube_t const &cube, vector<unsigned> &cobjs, int ignore_cobj, float toler, bool check_ccounter, int id_for_cobj_int) const;
	bool is_cobj_contained(point const &p1, point const &p2, point const &viewer, point const *const pts, unsigned npts, int ignore_cobj, int &cobj) const;
	void get_coll_line_cobjs(point const &pos1, point const &pos2, int ignore_cobj, vector<int> *cobjs, cobj_query_callback *cqc, bool occlude) const;
	void get_coll_sphere_cobjs(point const &center, float radius, int ignore_cobj, vert_coll_detector &vcd) const;
};


#endif // _COBJ_BSP_TREE_H_


