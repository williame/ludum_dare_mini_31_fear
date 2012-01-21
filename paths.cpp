#include "paths.hpp"
#include "barebones/xml.hpp"
#include "external/ogl-math/glm/gtx/closest_point.hpp"

static float distance(const glm::vec2& a,const glm::vec2& b) {
	float dx = a.x - b.x, dy = a.y - b.y;
	return sqrt(dx*dx+dy*dy);
}

float path_t::link_t::length() const { return ::distance(a->pos,b->pos); }

path_t::path_t(main_t& m): main(m), program(m.get_shared_program("path_t")),
	dirty(true), id_seq(0), active_node(false) {
	graphics_assert(program);
	glGenBuffers(sizeof(vbo)/sizeof(*vbo),vbo);
	uniform_mvp_matrix = main.get_uniform_loc(program,"MVP_MATRIX",GL_FLOAT_MAT4);
	uniform_colour = main.get_uniform_loc(program,"COLOUR",GL_FLOAT_VEC4);
	attrib_vertex = main.get_attribute_loc(program,"VERTEX",GL_FLOAT_VEC2);
}

path_t::node_t* path_t::get_node(int id,bool null) {
	for(nodes_t::iterator i=nodes.begin(); i!=nodes.end(); i++)
		if((*i)->id == id) return *i;
	if(null) return NULL;
	data_error("could not resolve path node ID " << id);
}

void path_t::load(xml_walker_t& xml) {
	for(int i=0; xml.get_child("node",i); i++, xml.up()) {
		const int id = xml.value_int("id");
		const float x = xml.value_float("x"), y = xml.value_float("y");
		if(get_node(id,true))
			data_error("duplicate path node ID " << id);
		nodes.push_back(new node_t(id,glm::vec2(x,y)));
		id_seq = std::max(id_seq,id+1);
	}
	for(int i=0; xml.get_child("link",i); i++, xml.up()) {
		node_t *a = get_node(xml.value_int("a")),
			*b = get_node(xml.value_int("b"));
		link_t* link = new link_t(a,b);
		links.push_back(link);
		a->links.push_back(link);
		b->links.push_back(link);

	}
}

void path_t::save(std::stringstream& xml) const {
	for(nodes_t::const_iterator i=nodes.begin(); i!=nodes.end(); i++)
		xml << "\t\t\t<node id=\"" << (*i)->id << "\" x=\"" << (*i)->pos.x << "\" y=\"" << (*i)->pos.y << "\"/>\n";
	for(links_t::const_iterator i=links.begin(); i!=links.end(); i++)
		xml << "\t\t\t<link a=\"" << (*i)->a->id << "\" b=\"" << (*i)->b->id << "\"/>\n";
}

void path_t::draw(const glm::mat4& projection,const glm::vec4& colour) {
	glUseProgram(program);
	glUniform4fv(uniform_colour,1,glm::value_ptr(const_cast<glm::vec4&>(colour)));
	glUniformMatrix4fv(uniform_mvp_matrix,1,false,glm::value_ptr(projection));
	glEnableVertexAttribArray(attrib_vertex);
	glCheck();
	if(links.size()) {
		glBindBuffer(GL_ARRAY_BUFFER,vbo[0]);
		if(dirty) {
			const size_t data_size = links.size()*4;
			GLfloat* const data = new GLfloat[data_size], *p = data;
			for(links_t::const_iterator l=links.begin(); l!=links.end(); l++) {
				*p++ = (*l)->a->pos.x;
				*p++ = (*l)->a->pos.y;
				*p++ = (*l)->b->pos.x;
				*p++ = (*l)->b->pos.y;
			}
			glBufferData(GL_ARRAY_BUFFER,data_size*sizeof(GLfloat),data,GL_STATIC_DRAW);
			glCheck();
			delete[] data;
		}
		glLineWidth(2.);
		glVertexAttribPointer(attrib_vertex,2,GL_FLOAT,GL_FALSE,0,0);
		glDrawArrays(GL_LINES,0,links.size()*2);
		glCheck();
	}
	if(active_node) {
		glBindBuffer(GL_ARRAY_BUFFER,vbo[2]);
		GLfloat data[2] = {active_node->pos.x,active_node->pos.y};
		glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);
		glPointSize(6.);
		glUniform4fv(uniform_colour,1,glm::value_ptr(glm::vec4(1,0,1,1)));
		glVertexAttribPointer(attrib_vertex,2,GL_FLOAT,GL_FALSE,0,0);
		glDrawArrays(GL_POINTS,0,1);
		glUniform4fv(uniform_colour,1,glm::value_ptr(const_cast<glm::vec4&>(colour)));
		glCheck();	
	}
	if(nodes.size()) {
		glBindBuffer(GL_ARRAY_BUFFER,vbo[1]);
		if(dirty) {
			const size_t data_size = nodes.size()*2;
			GLfloat* const data = new GLfloat[data_size], *p = data;
			for(nodes_t::const_iterator n=nodes.begin(); n!=nodes.end(); n++) {
				*p++ = (*n)->pos.x;
				*p++ = (*n)->pos.y;
			}
			glBufferData(GL_ARRAY_BUFFER,data_size*sizeof(GLfloat),data,GL_STATIC_DRAW);
			glCheck();
			delete[] data;
		}
		glPointSize(4.);
		glVertexAttribPointer(attrib_vertex,2,GL_FLOAT,GL_FALSE,0,0);
		glDrawArrays(GL_POINTS,0,nodes.size());
		glCheck();
	}
	dirty = false;
}

path_t::node_t* path_t::nearest(const glm::vec2& p,float threshold) {
	node_t* nearest = NULL;
	for(nodes_t::iterator n=nodes.begin(); n!=nodes.end(); n++) {
		const float d = distance(p,(*n)->pos);
		if(d<threshold) {
			nearest = *n;
			threshold = d;
		}
	}
	return nearest;
}

path_t::link_t* path_t::nearest_link(const glm::vec2& p,float threshold) {
	link_t* nearest = NULL;
	for(links_t::iterator l=links.begin(); l!=links.end(); l++) {
		const glm::vec3 n = glm::closestPointOnLine(glm::vec3(p,0),glm::vec3((*l)->a->pos,0),glm::vec3((*l)->b->pos,0));
		const float d = distance(p,glm::vec2(n.x,n.y));
		if(d<threshold) {
			nearest = *l;
			threshold = d;
		}
	}
	return nearest;
}

void path_t::on_mouse_down(int x,int y,main_t::mouse_button_t button,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse) {
	const glm::vec2 pos(x,y);
	if(button == main_t::MOUSE_DRAG) {
		if(active_node) {
			active_node->pos = pos;
			dirty = true;
		}
	} else if(node_t* new_active_node = nearest(pos)) {
		if(active_node) { // if they are not linked, join them
			bool joined = false;
			for(links_t::iterator l=active_node->links.begin(); !joined && l!=active_node->links.end(); l++)
				joined = ((*l)->a == new_active_node) || ((*l)->b == new_active_node);
			if(!joined) {
				link_t* link = new link_t(active_node,new_active_node);
				links.push_back(link);
				active_node->links.push_back(link);
				new_active_node->links.push_back(link);
				dirty = true;
			}
		}
		active_node = new_active_node;
	} else if(link_t* link = nearest_link(pos)) {
		active_node = new node_t(id_seq++,pos);
		nodes.push_back(active_node);
		active_node->links.push_back(link);
		node_t* b = link->b;
		b->links.erase(std::find(b->links.begin(),b->links.end(),link));
		link->b = active_node;
		link = new link_t(active_node,b);
		links.push_back(link);
		active_node->links.push_back(link);
		b->links.push_back(link);
		dirty = true;
	} else {
		node_t* new_active_node = new node_t(id_seq++,pos);
		if(active_node) {
			link_t* link = new link_t(active_node,new_active_node);
			links.push_back(link);
			active_node->links.push_back(link);
			new_active_node->links.push_back(link);
		}
		nodes.push_back(new_active_node);
		dirty = true;
		active_node = new_active_node;
	}
}

void path_t::on_mouse_up(int x,int y,main_t::mouse_button_t button,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse) {}

bool path_t::on_key_down(short code,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse) { return false; }

bool path_t::on_key_up(short code,const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse) {
	switch(code) {
	case main_t::KEY_BACKSPACE:
		if(map.any()) return false;
		if(active_node) {
			for(links_t::iterator l=active_node->links.begin(); l!=active_node->links.end(); l++) {
				link_t* link = *l;
				links.erase(std::find(links.begin(),links.end(),link));
				if(link->a == active_node)
					link->b->links.erase(std::find(link->b->links.begin(),link->b->links.end(),link));
				else if(link->b == active_node)
					link->a->links.erase(std::find(link->a->links.begin(),link->a->links.end(),link));
				else
					panic("wtf");
			}
			nodes.erase(std::find(nodes.begin(),nodes.end(),active_node));
			delete active_node;
			active_node = NULL;
			dirty = true;
		}
		return true;
	default:
		return false;
	}
}

