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
	struct artwork_t;
private:
	friend struct artwork_t;
	void on_ready(artwork_t* artwork);
	bool is_ready() const;
	void save();
	void play();
	void play_tick(float step);
	artwork_t* load_asset(xml_walker_t& xml);
	enum {
		LOAD_GAME_XML,
	};
	enum {
		MODE_LOAD,
		MODE_PLACE_OBJECT,
		MODE_EDIT_OBJECT,
		MODE_FLOOR,
		MODE_CEILING,
		MODE_HOT,
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
	struct hot_t {
		hot_t(): type(BAD) {}
		glm::vec2 a, b;
		enum {
			STOP,
			BAD
		} type;
		void draw(main_t& main,const glm::mat4& mvp,glm::vec4 colour);
		void normalise() {
			const float minx(std::min(a.x,b.x)), miny(std::min(a.y,b.y)),
				maxx(std::max(a.x,b.x)), maxy(std::max(a.y,b.y));
			a = glm::vec2(minx,miny);
			b = glm::vec2(maxx,maxy);
		}
		bool contains(const glm::vec2& p) const {
			return (p.x >= a.x && p.y >= a.y && p.x < b.x && p.y < b.y);
		}
	};
	typedef std::vector<hot_t> hots_t;
	hots_t hots;
	hot_t new_hot, active_hot;
	bool draw_hot;
	bool mouse_down;
	float mouse_x, mouse_y;
	static const float PAN_RATE;
};

const float main_game_t::PAN_RATE = 800; // px/sec

struct main_game_t::artwork_t {
	enum class_t {
		CLS_BACK = 150,
		CLS_MONSTER = 90,
		CLS_PLAYER = 89,
	};
	virtual ~artwork_t() {}
	main_game_t& game;
	const std::string id;
	const class_t cls;
	const glm::mat4 tx;
	const glm::vec4 anchor;
	const float speed;
	virtual void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour = glm::vec4(1,1,1,1)) = 0;
	virtual artwork_t* get_child(const std::string& id) { return this; }
	virtual artwork_t* get_child(size_t i) { return NULL; }
	virtual void bounds(glm::vec3& min,glm::vec3& max) = 0;
	virtual void save(std::stringstream& xml) = 0;
	bool is_ready() const { return _ready; }
protected:
	void on_ready(bool ok) {
		if(!ok) data_error("failed to load " << id);
		_ready = true;
		game.on_ready(this);
	}
	artwork_t(main_game_t& g,const std::string& id_,class_t c,const glm::mat4& t,const glm::vec4& a,float s):
		game(g), id(id_), cls(c), tx(t), anchor(a), speed(s), _ready(false) {}
	bool _ready;
};	
	
struct g3d_artwork_t: public main_game_t::artwork_t, private g3d_t::loaded_t {
	g3d_artwork_t(main_game_t& main,const std::string& id_,const std::string& p,class_t c,float sf,float sp,const glm::vec3& a,float al):
		artwork_t(main,id_,c,glm::scale(glm::vec3(sf,sf,sf)),glm::scale(glm::vec3(sf,sf,sf))*glm::vec4(a,1),sp),
		path(p), scale_factor(sf), animation_length(al),
		g3d(main,p,this) {}
	const std::string path;
	const float scale_factor, animation_length;
	g3d_t g3d;
	void on_g3d_loaded(g3d_t& g3d,bool ok,intptr_t data) {
		if(!ok) data_error("failed to load " << path);
		artwork_t::on_ready(this);
	}
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour) {
		const float anim_len = (animation_length>0?animation_length:2);
		const float t = fmod(time/anim_len,1);
		g3d.draw(t,projection,modelview,light0,colour);
	}
	void bounds(glm::vec3& min,glm::vec3& max) { return g3d.bounds(min,max); }
	void save(std::stringstream& xml) {
		xml << "\t\t<asset id=\"" << id << "\" type=\"g3d\" class=\"" <<
			(cls == artwork_t::CLS_BACK?"back":
			cls == artwork_t::CLS_PLAYER?"player":
				"monster") <<
			"\" path=\"" << path << "\" scale_factor=\"" << scale_factor << "\"";
		if(speed > 0)
			xml << " speed=\"" << speed << "\"";
		if(anchor.x != 0)
			xml << " anchor_x=\"" << anchor.x << "\"";
		if(anchor.y != 0)
			xml << " anchor_y=\"" << anchor.y << "\"";
		if(anchor.z != 0)
			xml << " anchor_z=\"" << anchor.z << "\"";
		if(animation_length > 0)
			xml << " animation_length=\"" << animation_length << "\"";
		xml << "/>\n";
	}
};

struct main_game_t::object_t {
	object_t(artwork_t& a,const glm::vec2& p):
		artwork(a), pos(p), dir(IDLE), state(WALKING), active_artwork(a.get_child("idle")), animation_start(a.game.now_secs()) {}
	artwork_t& artwork;
	glm::vec2 pos;
	enum {
		LEFT = -1,
		IDLE = 0,
		RIGHT = 1
	} dir, jump_dir;
	enum {
		WALKING,
		JUMPING,
	} state;
	double jump_gravity, jump_energy;
	artwork_t* active_artwork;
	double animation_start;
	glm::mat4 tx() {
		glm::mat4 tx(glm::translate(glm::vec3(pos,-artwork.cls))*artwork.tx);
		if(dir == LEFT)
			tx *= glm::rotate(270.0f,glm::vec3(0,1,0));
		else if(dir == RIGHT)
			tx *= glm::rotate(90.0f,glm::vec3(0,1,0));
		return tx;
	}
};

void main_game_t::hot_t::draw(main_t& main,const glm::mat4& mvp,glm::vec4 colour) {
	const GLfloat data[4*2] = {
		a.x,a.y,
		b.x,a.y,
		b.x,b.y,
		a.x,b.y};
	GLuint program = main.get_shared_program("path_t"),
		uniform_mvp_matrix = main.get_uniform_loc(program,"MVP_MATRIX",GL_FLOAT_MAT4),
		uniform_colour = main.get_uniform_loc(program,"COLOUR",GL_FLOAT_VEC4),
		attrib_vertex = main.get_attribute_loc(program,"VERTEX",GL_FLOAT_VEC2),
		vbo;
	glUseProgram(program);
	glUniform4fv(uniform_colour,1,glm::value_ptr(colour));
	glUniformMatrix4fv(uniform_mvp_matrix,1,false,glm::value_ptr(mvp));
	glEnableVertexAttribArray(attrib_vertex);
	glCheck();
	glGenBuffers(1,&vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);
	glLineWidth(2.);
	glVertexAttribPointer(attrib_vertex,2,GL_FLOAT,GL_FALSE,0,0);
	glDrawArrays(GL_LINE_LOOP,0,4);
	glDeleteBuffers(1,&vbo);
}

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

main_game_t::artwork_t* main_game_t::load_asset(xml_walker_t& xml) {
	const std::string id = xml.value_string("id"),
		type = xml.value_string("type"), 
		scls = xml.value_string("class");
	main_game_t::artwork_t::class_t cls;
	if(scls == "back") cls = main_game_t::artwork_t::CLS_BACK;
	else if(scls=="monster") cls = main_game_t::artwork_t::CLS_MONSTER;
	else if(scls=="player") cls = main_game_t::artwork_t::CLS_PLAYER;
	else data_error(scls << " is not a supported artwork class");
	const glm::vec3 anchor(
		xml.has_key("anchor_x")? xml.value_float("anchor_x"):0,
		xml.has_key("anchor_y")? xml.value_float("anchor_y"):0,
		xml.has_key("anchor_z")? xml.value_float("anchor_z"):0);
	const float speed = xml.has_key("speed")? xml.value_float("speed"):0;
	if(type == "g3d") {
		const std::string path = xml.value_string("path");
		const float scaler = xml.has_key("scale_factor")? xml.value_float("scale_factor"):1.0;
		const float animation_length = xml.has_key("animation_length")? xml.value_float("animation_length"): 0;
		std::cout << "loading G3D " << path << std::endl;
		return new g3d_artwork_t(*this,id,path,cls,scaler,speed,anchor,animation_length);
	} else
		data_error("unsupported artwork type "<<type);
}

void main_game_t::on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) {
	if(!ok) data_error("could not load " << name);
	switch(data) {
	case LOAD_GAME_XML: {
		game_xml = xml_parser_t(name,bytes);
		xml_walker_t xml(game_xml.walker());
		xml.check("game").get_child("artwork");
		for(int i=0; xml.get_child("asset",i); i++, xml.up()) {
			artwork_t* a = load_asset(xml);
			if(artwork.find(a->id) != artwork.end())
				data_error("dupicate asset ID " << a->id);
			artwork[a->id] = a;
			if(!active_model) active_model = a;
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
		for(int i=0; xml.get_child("hot",i); i++, xml.up()) {
			new_hot.a.x = xml.value_float("x1");
			new_hot.a.y = xml.value_float("y1");
			new_hot.b.x = xml.value_float("x2");
			new_hot.b.y = xml.value_float("y2");
			new_hot.normalise();
			const std::string type = xml.value_string("type");
			if(type == "stop") new_hot.type = hot_t::STOP;
			else data_error("unsupported hot type " << type);
			hots.push_back(new_hot);
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
	static double first_tick = now_secs(), last_tick;
	const double now = this->now_secs(),
		elapsed = now-first_tick,
		since_last = now-last_tick;
	if(mode == MODE_PLAY) {
		play_tick(since_last);
		screen_centre = player->pos;
	} else
		screen_centre += pan_rate * glm::vec2(since_last,since_last);
	const glm::mat4 projection(glm::ortho<float>(
		screen_centre.x-width/2,screen_centre.x+width/2,
		screen_centre.y-height/2,screen_centre.y+height/2, // y increases upwards
		1,300));
	const glm::vec3 light0(10,10,10);
	// show all the objects
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		(*i)->artwork.draw(elapsed-(*i)->animation_start,projection,(*i)->tx(),light0);
	if(mode != MODE_PLAY) {
		// show active model on top for editing
		if((mode == MODE_PLACE_OBJECT) && active_model && mouse_down) {
			active_model->draw(elapsed,projection,
				glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2,-50))*
					active_model->tx,
				light0,glm::vec4(1,.6,.6,.6));
		}
		if((mode == MODE_HOT) && draw_hot)
			new_hot.draw(*this,projection,glm::vec4(1,1,0,1));
		for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++)
			i->draw(*this,projection,glm::vec4(0,1,0,1));
		// floor and ceiling ## hide for production	
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
	glm::vec2 move(player->active_artwork->speed * step * (player->state==object_t::JUMPING?player->jump_dir:player->dir),0),
		player_pos(glm::vec2(player->artwork.anchor.x,player->artwork.anchor.y)+player->pos);
	float floor_y;
	if(floor->y_at(player_pos,floor_y,true)) {
		if(player->state == object_t::JUMPING) {
			player->jump_gravity += 4.0*step;
			const double jump_up = player->jump_energy*step - player->jump_gravity;
			if(jump_up+player_pos.y < floor_y) {
				move.y = floor_y-player_pos.y;
				player->state = object_t::WALKING;
			} else
				move.y = jump_up;
		} else {
			assert(player->state == object_t::WALKING);
			move.y = floor_y-player_pos.y;
		}
	} else // else path says stop; play a bump sound?
		move.x = 0;
	glm::vec2 new_pos = player->pos + move;
	for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++)
		if(i->contains(new_pos))
			switch(i->type) {
			case hot_t::STOP:
				move.x = 0; // stop lateral movement in that direction
				break;
			default:;
			}
	player->pos += move;
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
		a->second->save(xml);
	xml << "\t</artwork>\n\t<level>\n";
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		xml << "\t\t<object asset=\"" << (*i)->artwork.id << "\" x=\"" << (*i)->pos.x << "\" y=\"" << (*i)->pos.y << "\"/>\n";
	for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++) {
		xml << "\t\t<hot x1=\"" << i->a.x << "\" y1=\"" << i->a.y << "\" x2=\"" << i->b.x << "\" y2=\"" << i->b.y << "\" type=\"";
		if(i->type == hot_t::STOP) xml << "stop";
		else data_error(i->type);
		xml << "\"/>\n";
	}
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
	glClearColor(0,0,0,1);
	std::cout << "Playing game!" << std::endl;
}

bool main_game_t::on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	if(mode == MODE_PLAY) {
		switch(code) {
		case KEY_UP:
			if(player->state == object_t::WALKING) {
				player->jump_gravity = 0;
				player->jump_energy = 160;
				player->jump_dir = player->dir;
				player->state = object_t::JUMPING;
			}
			break;
		case KEY_LEFT:
			player->dir = object_t::LEFT;
			break;
		case KEY_RIGHT:
			player->dir = object_t::RIGHT;
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
	case 'h': mode = MODE_HOT; draw_hot = false; std::cout << "HOT ZONE MODE" << std::endl; return true;
	case 'f': mode = MODE_FLOOR; std::cout << "FLOOR MODE" << std::endl; return true;
	case 'c': mode = MODE_CEILING; std::cout << "CEILING MODE" << std::endl; return true;
	case 'e': 
		active_object = NULL;
		mode = MODE_EDIT_OBJECT;
		std::cout << "OBJECT EDIT MODE " << (active_object?active_object->artwork.id:"<no selection>") << std::endl; 
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
		std::cout << "OBJECT PLACEMENT MODE " << active_model->id << std::endl; 
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
		case KEY_UP:
			player->jump_energy *= 0.6; // stop them jumping full-apogee
			break;
		case KEY_LEFT:
		case KEY_RIGHT:
			player->dir = object_t::IDLE;
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
					std::cout << "DELETING OBJECT " << active_object->artwork.id << std::endl;
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
		case MODE_HOT:
			if(mouse_down || !draw_hot) return false;
			switch(code) {
			case 'x':
				new_hot.type = hot_t::STOP;
				std::cout << "HOT is STOP" << std::endl;
				break;
			default:
				return false;
			} 
			new_hot.normalise();
			hots.push_back(new_hot);
			draw_hot = false;
			return true;
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
				active_object_anchor = pos;
			}
		} else {
			active_object = NULL;
			const glm::vec2 pos(mapped_x,mapped_y);
			for(objects_t::iterator o=objects.begin(); o!=objects.end(); o++) {
				glm::vec3 bl1, tr1;
				(*o)->artwork.bounds(bl1,tr1);
				const glm::mat4 tx = (*o)->tx();
				const glm::vec4 bl = tx * glm::vec4(bl1,1);
				const glm::vec4 tr = tx * glm::vec4(tr1,1);
				if((pos.x>=bl.x) &&
					(pos.y>=bl.y) &&
					(pos.x<tr.x) &&
					(pos.y<tr.y)) {
					active_object = *o;
					active_object_anchor = pos;
					break;
				}
			}
			std::cout << "OBJECT EDIT MODE " << (active_object?active_object->artwork.id:"<no selection>") << std::endl; 
		}
		break;
	case MODE_FLOOR:
		floor->on_mouse_down(mapped_x,mapped_y,button,map,mouse);
		break;
	case MODE_CEILING:
		ceiling->on_mouse_down(mapped_x,mapped_y,button,map,mouse);
		break;
	case MODE_HOT:
		if(!draw_hot)
			new_hot.a = glm::vec2(mapped_x,mapped_y);
		draw_hot = true;
		new_hot.b = glm::vec2(mapped_x,mapped_y);
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
			std::cout << "creating new object of " << active_model->id << std::endl;
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
