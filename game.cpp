#include <iostream>
#include <map>
#include <memory>

#ifndef __native_client__
	#include <fstream>
#endif

#include "barebones/main.hpp"
#include "barebones/xml.hpp"
#include "barebones/g3d.hpp"
#include "external/ogl-math/glm/gtx/transform.hpp"
#include "external/ogl-math/glm/gtx/closest_point.hpp"

void create_shaders(main_t& main); // shaders.cpp

const char* const main_t::game_name = "Ludum Dare Mini 31 - Fear"; // window titles etc

class main_game_t: public main_t, private main_t::file_io_t {
public:
	main_game_t(void* platform_ptr): main_t(platform_ptr),
		mode(MODE_LOAD), active_model(NULL), mouse_down(false) {}
	void init();
	bool tick();
	void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data);
	// debug just print state
	bool on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
private:
	struct artwork_t;
	friend struct artwork_t;
	void on_ready(artwork_t* artwork);
	bool is_ready() const;
	void save();
	enum {
		LOAD_GAME_XML,
	};
	enum {
		MODE_LOAD,
		MODE_OBJECT,
		MODE_FLOOR,
		MODE_CEILING,
	} mode;
	xml_parser_t game_xml;
	glm::vec2 screen_centre;
	typedef std::map<std::string,artwork_t*> artworks_t;
	artworks_t artwork;
	artwork_t* active_model;
	struct path_t;
	std::auto_ptr<path_t> floor, ceiling;
	struct object_t;
	typedef std::vector<object_t*> objects_t;
	objects_t objects;
	glm::vec2 pan_rate;
	bool mouse_down;
	float mouse_x, mouse_y;
	static const float PAN_RATE;
};

const float main_game_t::PAN_RATE = 10;

struct main_game_t::artwork_t: public g3d_t, public g3d_t::loaded_t {
	enum class_t {
		CLS_BACK = 150,
		CLS_MONSTER = 90,
	};
	artwork_t(main_game_t& main,const std::string& id_,const std::string& p,class_t c,float s):
		g3d_t(main,p,this), game(main), id(id_), path(p), cls(c), tx(glm::scale(glm::vec3(s,s,s))), scale_factor(s) {}
	main_game_t& game;
	const std::string id, path;
	class_t cls;
	const glm::mat4 tx;
	const float scale_factor;
	void on_g3d_loaded(g3d_t& g3d,bool ok,intptr_t data) {
		if(!ok) data_error("failed to load " << filename);
		game.on_ready(this);
	}
};

struct main_game_t::object_t {
	object_t(artwork_t& a,const glm::vec2& p):
		artwork(a), pos(p), tx(glm::translate(glm::vec3(p,-a.cls))*a.tx) {}
	artwork_t& artwork;
	glm::vec2 pos;
	glm::mat4 tx;
};

static float distance(const glm::vec2& a,const glm::vec2& b) {
	float dx = a.x - b.x, dy = a.y - b.y;
	return sqrt(dx*dx+dy*dy);
}

struct main_game_t::path_t {
	path_t(main_game_t& g): game(g), program(game.get_shared_program("path_t")),
		dirty(true), id_seq(0), active_node(false) {
		graphics_assert(program);
		glGenBuffers(sizeof(vbo)/sizeof(*vbo),vbo);
		uniform_mvp_matrix = game.get_uniform_loc(program,"MVP_MATRIX",GL_FLOAT_MAT4);
		uniform_colour = game.get_uniform_loc(program,"COLOUR",GL_FLOAT_VEC4);
		attrib_vertex = game.get_attribute_loc(program,"VERTEX",GL_FLOAT_VEC2);
	}
	main_game_t& game;
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
		float length() const { return ::distance(a->pos,b->pos); }
	};
	typedef std::vector<node_t*> nodes_t;
	nodes_t nodes;
	int id_seq;
	node_t* active_node;
	links_t links;
	node_t* get_node(int id,bool null=false) {
		for(nodes_t::iterator i=nodes.begin(); i!=nodes.end(); i++)
			if((*i)->id == id) return *i;
		if(null) return NULL;
		data_error("could not resolve path node ID " << id);
	}
	void load(xml_walker_t& xml) {
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
	void save(std::stringstream& xml) const {
		for(nodes_t::const_iterator i=nodes.begin(); i!=nodes.end(); i++)
			xml << "\t\t\t<node id=\"" << (*i)->id << "\" x=\"" << (*i)->pos.x << "\" y=\"" << (*i)->pos.y << "\"/>\n";
		for(links_t::const_iterator i=links.begin(); i!=links.end(); i++)
			xml << "\t\t\t<link a=\"" << (*i)->a->id << "\" b=\"" << (*i)->b->id << "\"/>\n";
	}
	void draw(const glm::mat4& projection,const glm::vec4& colour) {
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
	node_t* nearest(const glm::vec2& p,float threshold = 4) {
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
	link_t* nearest_link(const glm::vec2& p,float threshold = 4) {
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
	void on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
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
	void on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {}
	bool on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) { return false; }
	bool on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
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
};

void main_game_t::init() {
	create_shaders(*this);
	glClearColor(1,0,0,1);
	read_file("data/game.xml",this,LOAD_GAME_XML);
}

bool main_game_t::is_ready() const {
	for(artworks_t::const_iterator a=artwork.begin(); a!=artwork.end(); a++)
		if(!a->second->is_ready())
			return false;
	return true;
}

void main_game_t::on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) {
	if(!ok) data_error("could not load " << name);
	switch(data) {
	case LOAD_GAME_XML: {
		game_xml = xml_parser_t(name,bytes);
		xml_walker_t xml(game_xml.walker());
		xml.check("game").get_child("artwork");
		for(int i=0; xml.get_child("asset",i); i++, xml.up()) {
			const std::string id = xml.value_string("id"),
				type = xml.value_string("type"), 
				scls = xml.value_string("class"),
				path = xml.value_string("path");
			const float scaler = xml.has_key("scale_factor")? xml.value_float("scale_factor"):1.0;
			artwork_t::class_t cls;
			if(scls == "back") cls = artwork_t::CLS_BACK;
			else if(scls=="monster") cls = artwork_t::CLS_MONSTER;
			else data_error(scls << " is not a supported artwork class");
			if(artwork.find(id) != artwork.end())
				data_error("dupicate asset ID " << id);
			if(type == "g3d") {
				std::cout << "loading G3D " << path << std::endl;
				artwork[id] = new artwork_t(*this,id,path,cls,scaler);
				if(!active_model) active_model = artwork[id];
			} else
				data_error("unsupported artwork type "<<type);
		}
		xml.up();
	} break;
	default:
		data_error("stray on_io(" << name << ',' << data << ')');
	}
}

void main_game_t::on_ready(artwork_t*) {
	if(is_ready()) {
		std::cout << "artwork all loaded" << std::endl;
		mode = MODE_FLOOR;
		xml_walker_t xml(game_xml.walker());
		xml.check("game").get_child("level");
		for(int i=0; xml.get_child("object",i); i++, xml.up()) {
			const std::string asset = xml.value_string("asset");
			const float x = xml.value_float("x"), y = xml.value_float("y");
			if(artwork.find(asset) == artwork.end())
				data_error("unresolved asset ID " << asset);
			objects.push_back(new object_t(*artwork[asset],glm::vec2(x,y)));
		}
		xml.get_child("floor");
		floor.reset(new path_t(*this));
		floor->load(xml);
		xml.up().get_child("ceiling");
		ceiling.reset(new path_t(*this));
		ceiling->load(xml);
		xml.up();
		glClearColor(1,1,1,1);
	}
}

bool main_game_t::tick() {
	static float time = 0.0;
	time += 0.01;
	if(time > 1.0) time -= 1.0;
	screen_centre += pan_rate;
	const glm::mat4 projection(glm::ortho<float>(
		screen_centre.x-width/2,screen_centre.x+width/2,
		screen_centre.y-height/2,screen_centre.y+height/2, // y increases upwards
		1,200));
	const glm::vec3 light0(10,10,10);
	// show all the objects
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		(*i)->artwork.draw(time,projection,(*i)->tx,light0);
	// show active model on top for editing
	if((mode == MODE_OBJECT) && active_model && mouse_down) {
		active_model->draw(time,projection,
			glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2,-50))*active_model->tx,
			light0,glm::vec4(1,.6,.6,.6));
	}
	// floor and ceiling
	if(floor.get())
		floor->draw(projection,glm::vec4(1,0,0,1));
	if(ceiling.get())
		ceiling->draw(projection,glm::vec4(1,1,0,1));
	return true; // return false to exit program
}

void main_game_t::save() {
	if(mode == MODE_LOAD) {
		std::cout << "cannot save whilst loading" << std::endl;
		return;
	}
	std::cout << "saving..." << std::endl;
	std::stringstream xml(std::ios_base::out|std::ios_base::ate);
	xml << "<game>\n\t<artwork>\n";
	for(artworks_t::iterator a=artwork.begin(); a!=artwork.end(); a++)
		xml << "\t\t<asset id=\"" << a->first << "\" type=\"g3d\" class=\"" <<
			(a->second->cls == artwork_t::CLS_BACK?"back":"monster") <<
			"\" path=\"" << a->second->path << "\" scale_factor=\"" << a->second->scale_factor << "\"/>\n";
	xml << "\t</artwork>\n\t<level>\n";
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		xml << "\t\t<object asset=\"" << (*i)->artwork.id << "\" x=\"" << (*i)->pos.x << "\" y=\"" << (*i)->pos.y << "\"/>\n"; 
	xml << "\t\t<floor>\n";
	floor->save(xml);
	xml << "\t\t</floor>\n\t\t<ceiling>\n";
	ceiling->save(xml);
	xml << "\t\t</ceiling>\n";
	xml << "\t</level>\n</game>\n";
#ifdef __native_client__
	std::cout << xml.str();
#else
	std::fstream out("data/game.xml",std::fstream::out);
	out << xml.str();
	out.close();
#endif
}

bool main_game_t::on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	switch(code) {
	case KEY_LEFT: pan_rate.x = -PAN_RATE; return true;
	case KEY_RIGHT: pan_rate.x = PAN_RATE; return true;
	case KEY_UP: pan_rate.y = PAN_RATE; return true;
	case KEY_DOWN: pan_rate.y = -PAN_RATE; return true;
	case 'o': mode = MODE_OBJECT; std::cout << "OBJECT MODE" << std::endl; return true;
	case 'f': mode = MODE_FLOOR; std::cout << "FLOOR MODE" << std::endl; return true;
	case 'c': mode = MODE_CEILING; std::cout << "CEILING MODE" << std::endl; return true;
	default:
		switch(mode) {
		case MODE_OBJECT:
			if(code == ' ') {
				if(!artwork.size()) return false;
				bool next = false;
				for(artworks_t::iterator m=artwork.begin(); m!=artwork.end(); m++)
					if(next) {
						active_model = m->second;
						next = false;
						break;
					} else
						next = m->second == active_model;
				if(next)
					active_model = artwork.begin()->second;
			}
			return true;
		case MODE_FLOOR:
			return floor->on_key_down(code,map,mouse);
		case MODE_CEILING:
			return ceiling->on_key_down(code,map,mouse);
		default: return false;
		}		
	}
}

bool main_game_t::on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	switch(code) {
	case KEY_LEFT:
	case KEY_RIGHT: pan_rate.x = 0; return true;
	case KEY_UP:
	case KEY_DOWN: pan_rate.y = 0; return true;
	case 's': if(map.none()) save(); return true;
	default:
		switch(mode) {
		case MODE_FLOOR:
			return floor->on_key_up(code,map,mouse);
		case MODE_CEILING:
			return ceiling->on_key_up(code,map,mouse);
		default: return false;
		}
	}
}

bool main_game_t::on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	mouse_down = true;
	mouse_x = x;
	mouse_y = y;
	const int mapped_x = screen_centre.x+mouse_x-width/2, mapped_y = screen_centre.y-mouse_y+height/2;
	switch(mode) {
	case MODE_FLOOR:
		floor->on_mouse_down(mapped_x,mapped_y,button,map,mouse);
		break;
	case MODE_CEILING:
		ceiling->on_mouse_down(mapped_x,mapped_y,button,map,mouse);
		break;
	default:;
	}
	return true;
}

bool main_game_t::on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	mouse_down = false;
	mouse_x = x;
	mouse_y = y;
	const int mapped_x = screen_centre.x+mouse_x-width/2, mapped_y = screen_centre.y-mouse_y+height/2;
	switch(mode) {
	case MODE_LOAD:
		break;
	case MODE_FLOOR:
		floor->on_mouse_up(mapped_x,mapped_y,button,map,mouse);
		break;
	case MODE_CEILING:
		ceiling->on_mouse_up(mapped_x,mapped_y,button,map,mouse);
		break;
	case MODE_OBJECT:
		if(active_model) {
			std::cout << "creating new object of " << active_model->filename << std::endl;
			const glm::vec2 pos(mapped_x,mapped_y);
			objects.push_back(new object_t(*active_model,pos));
		} else
			std::cout << "on_mouse_up without active_model" << std::endl;
		break;
	}
	return true;
}

main_t* main_t::create(void* platform_ptr,int argc,char** args) {
	return new main_game_t(platform_ptr);
}
