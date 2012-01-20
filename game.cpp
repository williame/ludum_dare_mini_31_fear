#include <iostream>
#include <map>

#include "barebones/main.hpp"
#include "barebones/xml.hpp"
#include "barebones/g3d.hpp"
#include "external/ogl-math/glm/gtx/transform.hpp"

void create_shaders(main_t& main); // shaders.cpp

const char* const main_t::game_name = "Ludum Dare Mini 31 - Fear"; // window titles etc

class main_game_t: public main_t, private main_t::file_io_t {
public:
	main_game_t(void* platform_ptr): main_t(platform_ptr),
		active_model(NULL), mouse_down(false) {}
	void init();
	bool tick();
	void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data);
	// debug just print state
	bool on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
private:
	enum {
		LOAD_GAME_XML,
	};
	xml_parser_t game_xml;
	glm::vec2 screen_centre;
	struct model_t;
	typedef std::map<std::string,model_t*> models_t;
	models_t models;
	model_t* active_model;
	glm::vec2 pan_rate;
	bool mouse_down;
	float mouse_x, mouse_y;
	static const float PAN_RATE;
};

const float main_game_t::PAN_RATE = 10;

struct main_game_t::model_t: public g3d_t, public g3d_t::loaded_t {
	model_t(main_t& main,const std::string& filename,float s):
		g3d_t(main,filename,this), tx(glm::scale(glm::vec3(s,s,s))) {}
	const glm::mat4 tx;
	void on_g3d_loaded(g3d_t& g3d,bool ok,intptr_t data) {
		if(!ok) data_error("failed to load " << filename);
	}
};

void main_game_t::init() {
	create_shaders(*this);
	glClearColor(.4,.2,.6,1.);
	glFrontFace(GL_CW); // we've got our y coords upside down
	read_file("data/game.xml",this,LOAD_GAME_XML);
}


void main_game_t::on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) {
	if(!ok) data_error("could not load " << name);
	switch(data) {
	case LOAD_GAME_XML: {
		game_xml = xml_parser_t(name,bytes);
		xml_walker_t xml(game_xml.walker());
		xml.check("game").get_child("artwork");
		for(int i=0; xml.get_child("art",i); i++, xml.up()) {
			const std::string id = xml.value_string("id"),
				type = xml.value_string("type"), 
				path = xml.value_string("path");
			const float scaler = xml.has_key("scale_factor")? xml.value_float("scale_factor"):1.0;
			if(models.find(id) != models.end())
				data_error("dupicate art ID " << id);
			if(type == "g3d") {
				models[id] = new model_t(*this,path,scaler);
				if(!active_model) active_model = models[id];
			} else
				data_error("unsupported artwork type "<<type);
		}
		xml.up();
	} break;
	default:
		data_error("stray on_io(" << name << ',' << data << ')');
	}
}

bool main_game_t::tick() {
	static float time = 0.0;
	time += 0.01;
	if(time > 1.0) time -= 1.0;
	screen_centre += pan_rate;
	const glm::mat4 projection(glm::ortho<float>(
		screen_centre.x-width/2,screen_centre.x+width/2,
		screen_centre.y+height/2,screen_centre.y-height/2,
		FLT_MIN,FLT_MAX));
	const glm::mat4 modelview(glm::translate(glm::vec3(0,0,50)));
	const glm::vec3 light0(10,10,10);
	for(models_t::iterator m=models.begin(); m!=models.end(); m++)
		m->second->draw(time,projection,modelview*m->second->tx,light0);
	// show active model
	if(active_model && mouse_down)
		active_model->draw(time,projection,
			glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y+mouse_y-height/2,25))*active_model->tx,
			light0,glm::vec4(1,0,.2,.4));
	return true; // return false to exit program
}

main_t* main_t::create(void* platform_ptr,int argc,char** args) {
	return new main_game_t(platform_ptr);
}

bool main_game_t::on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	switch(code) {
	case KEY_LEFT: pan_rate.x = PAN_RATE; return true;
	case KEY_RIGHT: pan_rate.x = -PAN_RATE; return true;
	case KEY_UP: pan_rate.y = PAN_RATE; return true;
	case KEY_DOWN: pan_rate.y = -PAN_RATE; return true;
	case ' ': {
			bool next = !active_model;
			for(models_t::iterator m=models.begin(); m!=models.end(); m++)
				if(next) {
					active_model = m->second;
					break;
				} else
					next = m->second == active_model;
			if(next)
				active_model = models.begin()->second;
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
	return true;
}
