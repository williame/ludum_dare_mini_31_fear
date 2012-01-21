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
		MODE_PLACE_OBJECT,
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
		CLS_BACK = 50,
		CLS_MONSTER = 30,
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

struct main_game_t::path_t {
	struct link_t;
	typedef std::vector<link_t*> links_t;
	struct node_t {
		node_t(int id_,const glm::vec2& p): id(id_), pos(p) {}
		const int id;
		const glm::vec2 pos;
		links_t links;
	};
	struct link_t {
		link_t(const node_t* a_,const node_t* b_): a(a_), b(b_) {}
		const node_t* const a;
		const node_t* const b;
		float distance() const {
			float dx = a->pos.x - b->pos.x, dy = a->pos.y - b->pos.y;
			return sqrt(dx*dx+dy*dy);
		}
	};
	typedef std::vector<node_t*> nodes_t;
	nodes_t nodes;
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
		mode = MODE_PLACE_OBJECT;
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
		floor.reset(new path_t());
		floor->load(xml);
		xml.up().get_child("ceiling");
		ceiling.reset(new path_t());
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
		1,90));
	const glm::vec3 light0(10,10,10);
	// show all the objects
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		(*i)->artwork.draw(time,projection,(*i)->tx,light0);
	// show active model on top for editing
	if((mode == MODE_PLACE_OBJECT) && active_model && mouse_down) {
		active_model->draw(time,projection,
			glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2,-20))*active_model->tx,
			light0,glm::vec4(1,.6,.6,.6));
	}
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
	case ' ': {
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
	} return true;
	default: return false;
	}
	return false;
}

bool main_game_t::on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	switch(code) {
	case KEY_LEFT:
	case KEY_RIGHT: pan_rate.x = 0; return true;
	case KEY_UP:
	case KEY_DOWN: pan_rate.y = 0; return true;
	case 's': save(); return true;
	default: return false;
	}
	return false;
}

bool main_game_t::on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	mouse_down = true;
	mouse_x = x;
	mouse_y = y;
	return true;
}

bool main_game_t::on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	mouse_down = false;
	if(active_model) {
		std::cout << "creating new object of " << active_model->filename << std::endl;
		const glm::vec2 pos(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2);
		objects.push_back(new object_t(*active_model,pos));
	} else
		std::cout << "on_mouse_up without active_model" << std::endl;
	return true;
}

main_t* main_t::create(void* platform_ptr,int argc,char** args) {
	return new main_game_t(platform_ptr);
}
