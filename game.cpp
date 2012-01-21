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

#include "paths.hpp"

void create_shaders(main_t& main); // shaders.cpp

const char* const main_t::game_name = "Ludum Dare Mini 31 - Fear"; // window titles etc

class main_game_t: public main_t, private main_t::file_io_t {
public:
	main_game_t(void* platform_ptr): main_t(platform_ptr),
		mode(MODE_LOAD), active_model(NULL), active_object(NULL), player(NULL), mouse_down(false) {}
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
	void play();
	void play_tick(float step);
	enum {
		LOAD_GAME_XML,
	};
	enum {
		MODE_LOAD,
		MODE_PLACE_OBJECT,
		MODE_EDIT_OBJECT,
		MODE_FLOOR,
		MODE_CEILING,
		MODE_PLAY
	} mode;
	xml_parser_t game_xml;
	glm::vec2 screen_centre;
	typedef std::map<std::string,artwork_t*> artworks_t;
	artworks_t artwork;
	artwork_t* active_model;
	std::auto_ptr<path_t> floor, ceiling;
	struct object_t;
	typedef std::vector<object_t*> objects_t;
	objects_t objects;
	object_t* active_object;
	glm::vec2 pan_rate, active_object_anchor;
	object_t* player;
	float player_dir;
	bool mouse_down;
	float mouse_x, mouse_y;
	static const float PAN_RATE;
};

static const glm::mat4
	ROT_LEFT(glm::rotate(90.0f,glm::vec3(0,1,0))),
	ROT_RIGHT(glm::rotate(270.0f,glm::vec3(0,1,0)));

const float main_game_t::PAN_RATE = 800; // px/sec

struct main_game_t::artwork_t: public g3d_t, public g3d_t::loaded_t {
	enum class_t {
		CLS_BACK = 150,
		CLS_MONSTER = 90,
		CLS_PLAYER = 89,
	};
	artwork_t(main_game_t& main,const std::string& id_,const std::string& p,class_t c,float sf,float sp,const glm::vec3& a):
		g3d_t(main,p,this), game(main), id(id_), path(p), cls(c),
		tx(glm::scale(glm::vec3(sf,sf,sf))),
		anchor(glm::scale(glm::vec3(sf,sf,sf))*glm::vec4(a,1)),
		scale_factor(sf), speed(sp) {}
	main_game_t& game;
	const std::string id, path;
	class_t cls;
	const glm::mat4 tx;
	const glm::vec4 anchor;
	const float scale_factor, speed;
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
	void draw_selection(const glm::mat4& projection,const glm::vec4& colour);
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
			const float speed = xml.has_key("speed")? xml.value_float("speed"):0;
			const glm::vec3 anchor(
				xml.has_key("anchor_x")? xml.value_float("anchor_x"):0,
				xml.has_key("anchor_y")? xml.value_float("anchor_y"):0,
				xml.has_key("anchor_z")? xml.value_float("anchor_z"):0);
			artwork_t::class_t cls;
			if(scls == "back") cls = artwork_t::CLS_BACK;
			else if(scls=="monster") cls = artwork_t::CLS_MONSTER;
			else if(scls=="player") cls = artwork_t::CLS_PLAYER;
			else data_error(scls << " is not a supported artwork class");
			if(artwork.find(id) != artwork.end())
				data_error("dupicate asset ID " << id);
			if(type == "g3d") {
				std::cout << "loading G3D " << path << std::endl;
				artwork[id] = new artwork_t(*this,id,path,cls,scaler,speed,anchor);
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
	static uint64_t first_tick = now(), last_tick;
	const uint64_t now = this->now();
	const double elapsed = (double)(now-first_tick)/1000000000, // seconds
		time = fmod(elapsed*.5,1), // cycle every 2 seconds
		since_last = (double)(now-last_tick)/1000000000;
	if(mode == MODE_PLAY) {
		play_tick(since_last);
		screen_centre = player->pos;
	} else
		screen_centre += pan_rate * glm::vec2(since_last,since_last);
	const glm::mat4 projection(glm::ortho<float>(
		screen_centre.x-width/2,screen_centre.x+width/2,
		screen_centre.y-height/2,screen_centre.y+height/2, // y increases upwards
		1,250));
	const glm::vec3 light0(10,10,10);
	// show all the objects
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		(*i)->artwork.draw(time,projection,(*i)->tx,light0);
	if(mode != MODE_PLAY) {
		// show active model on top for editing
		if((mode == MODE_PLACE_OBJECT) && active_model && mouse_down) {
			active_model->draw(time,projection,
				glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2,-50))*active_model->tx,
				light0,glm::vec4(1,.6,.6,.6));
		}
		// floor and ceiling
		if(floor.get())
			floor->draw(projection,glm::vec4(1,0,0,1));
		if(ceiling.get())
			ceiling->draw(projection,glm::vec4(1,1,0,1));
	}
	// done
	last_tick = now;
	return true; // return false to exit program
}

void main_game_t::play_tick(float step) {
	// move main player
	glm::vec2 move(player->artwork.speed * step * player_dir,0),
		player_pos(glm::vec2(player->artwork.anchor.x,player->artwork.anchor.y)+player->pos);
	float floor_y;
	if(floor->y_at(player_pos,floor_y,true)) {
		floor_y -= player_pos.y;
		std::cout << "player " << player_pos.x << ',' << player_pos.y << ", floor " << floor_y << std::endl;
		if(floor_y > player_pos.y)
			move.y = floor_y;
		else if(floor_y < player_pos.y)
			move.y = std::min(floor_y,step);
	} else {
		std::cerr << "LEVEL ERROR: player falls off world!" << std::endl;
		mode = MODE_FLOOR;
		return;
	}
	player->pos += move;
	player->tx = glm::translate(glm::vec3(move,0)) * player->tx;
}

void main_game_t::save() {
	if((mode == MODE_LOAD) || (mode == MODE_PLAY)) {
		std::cout << "cannot save in this mode" << std::endl;
		return;
	}
	std::cout << "saving..." << std::endl;
	std::stringstream xml(std::ios_base::out|std::ios_base::ate);
	xml << "<game>\n\t<artwork>\n";
	for(artworks_t::iterator a=artwork.begin(); a!=artwork.end(); a++)
		xml << "\t\t<asset id=\"" << a->first << "\" type=\"g3d\" class=\"" <<
			(a->second->cls == artwork_t::CLS_BACK?"back":
			a->second->cls == artwork_t::CLS_PLAYER?"player":
				"monster") <<
			"\" path=\"" << a->second->path << "\" scale_factor=\"" << a->second->scale_factor << 
			"\" speed=\"" << a->second->speed << "\"/>\n";
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

void main_game_t::play() {
	// find player object
	player = NULL;
	for(objects_t::iterator o=objects.begin(); o!=objects.end(); o++)
		if((*o)->artwork.cls == artwork_t::CLS_PLAYER) {
			if(player) {
				std::cerr << "There is more than one player!  Cannot play." << std::endl;
				player = NULL;
				return;
			} else
				player = *o;
		}
	if(!player) {
		std::cerr << "There is no player!  Cannot play." << std::endl;
		return;
	}
	if(!player->artwork.speed) {
		std::cerr << "The player cannot move!  Add a speed= parameter to the xml!" << std::endl;
		return;
	}
	mode = MODE_PLAY;
	pan_rate = glm::vec2(0,0);
	player_dir = 0;
	std::cout << "Playing game!" << std::endl;
}

bool main_game_t::on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	if(mode == MODE_PLAY) {
		switch(code) {
		case KEY_LEFT:
			player_dir = -1;
			break;
		case KEY_RIGHT:
			player_dir = 1;
			break;
		default:;
		}
		return true;
	}
	switch(code) {
	case KEY_LEFT: pan_rate.x = -PAN_RATE; return true;
	case KEY_RIGHT: pan_rate.x = PAN_RATE; return true;
	case KEY_UP: pan_rate.y = PAN_RATE; return true;
	case KEY_DOWN: pan_rate.y = -PAN_RATE; return true;
	case 'f': mode = MODE_FLOOR; std::cout << "FLOOR MODE" << std::endl; return true;
	case 'c': mode = MODE_CEILING; std::cout << "CEILING MODE" << std::endl; return true;
	case 'e': 
		active_object = NULL;
		mode = MODE_EDIT_OBJECT;
		std::cout << "OBJECT EDIT MODE " << (active_object?active_object->artwork.path:"<no selection>") << std::endl; 
		return true;
	case 'o':
		if(mode == MODE_PLACE_OBJECT) {
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
		mode = MODE_PLACE_OBJECT;
		std::cout << "OBJECT PLACEMENT MODE " << active_model->path << std::endl; 
		return true;
	case 'p':
		play();
		return true;
	default:
		switch(mode) {
		case MODE_EDIT_OBJECT:
			return false;
		case MODE_FLOOR:
			return floor->on_key_down(code,map,mouse);
		case MODE_CEILING:
			return ceiling->on_key_down(code,map,mouse);
		default: return false;
		}		
	}
}

bool main_game_t::on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	if(mode == MODE_PLAY) {
		switch(code) {
		case KEY_LEFT:
		case KEY_RIGHT:
			player_dir = 0;
			break;
		default:;
		}
		return true;
	}
	switch(code) {
	case KEY_LEFT:
	case KEY_RIGHT: pan_rate.x = 0; return true;
	case KEY_UP:
	case KEY_DOWN: pan_rate.y = 0; return true;
	case 's': if(map.none()) save(); return true;
	default:
		switch(mode) {
		case MODE_EDIT_OBJECT:
			if(code == KEY_BACKSPACE) {
				if(active_object) {
					std::cout << "DELETING OBJECT " << active_object->artwork.path << std::endl;
					objects.erase(std::find(objects.begin(),objects.end(),active_object));
					delete active_object;
					active_object = NULL;
					return true;
				}
			}
			return false;
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
	case MODE_EDIT_OBJECT:
		if(button == MOUSE_DRAG) {
			if(active_object) {
				const glm::vec2 pos(mapped_x,mapped_y),
					ofs(pos-active_object_anchor);
				active_object->pos += ofs;
				active_object->tx = glm::translate(glm::vec3(ofs,0)) * active_object->tx;
				active_object_anchor = pos;
			}
		} else {
			active_object = NULL;
			const glm::vec2 pos(mapped_x,mapped_y);
			for(objects_t::iterator o=objects.begin(); o!=objects.end(); o++) {
				glm::vec3 bl1, tr1;
				(*o)->artwork.bounds(bl1,tr1);
				const glm::vec4 bl = (*o)->tx * glm::vec4(bl1,1);
				const glm::vec4 tr = (*o)->tx * glm::vec4(tr1,1);
				if((pos.x>=bl.x) &&
					(pos.y>=bl.y) &&
					(pos.x<tr.x) &&
					(pos.y<tr.y)) {
					active_object = *o;
					active_object_anchor = pos;
					break;
				}
			}
			std::cout << "OBJECT EDIT MODE " << (active_object?active_object->artwork.path:"<no selection>") << std::endl; 
		}
		break;
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
	case MODE_PLACE_OBJECT:
		if(active_model) {
			std::cout << "creating new object of " << active_model->filename << std::endl;
			const glm::vec2 pos(mapped_x,mapped_y);
			objects.push_back(new object_t(*active_model,pos));
		} else
			std::cout << "on_mouse_up without active_model" << std::endl;
		break;
	default:;
	}
	return true;
}

main_t* main_t::create(void* platform_ptr,int argc,char** args) {
	return new main_game_t(platform_ptr);
}
