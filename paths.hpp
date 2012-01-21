#ifndef __PATHS_HPP__
#define __PATHS_HPP__

#include "barebones/main.hpp"
#include "external/ogl-math/glm/glm.hpp"
#include "external/ogl-math/glm/gtc/type_ptr.hpp"

class xml_walker_t;

class path_t {
public:
	path_t(main_t& main);
	main_t& main;
	void load(xml_walker_t& xml);
	void save(std::stringstream& xml) const;
	void draw(const glm::mat4& projection,const glm::vec4& colour);
	bool y_at(const glm::vec2& p,float& y,bool down) const;
	void on_mouse_down(int x,int y,main_t::mouse_button_t button,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse);
	void on_mouse_up(int x,int y,main_t::mouse_button_t button,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse);
	bool on_key_down(short code,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse);
	bool on_key_up(short code,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse);
private:
	GLuint program, vbo[3], uniform_colour, uniform_mvp_matrix, attrib_vertex;
	bool dirty;
	struct link_t;
	typedef std::vector<link_t*> links_t;
	struct node_t {
		node_t(int id_,const glm::vec2& p): id(id_), pos(p) {}
		const int id;
		glm::vec2 pos;
		links_t links;
	};
	struct link_t {
		link_t(node_t* a_,node_t* b_): a(a_), b(b_) {}
		node_t* a;
		node_t* b;
		float length() const;
	};
	typedef std::vector<node_t*> nodes_t;
	nodes_t nodes;
	int id_seq;
	node_t* active_node;
	links_t links;
	node_t* get_node(int id,bool null=false);
	node_t* nearest(const glm::vec2& p,float threshold = 4);
	link_t* nearest_link(const glm::vec2& p,float threshold = 4);
};

#endif//__PATHS_HPP__
