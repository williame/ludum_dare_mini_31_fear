#include <iostream>
#include <map>
#include <memory>

#ifndef __native_client__
	#include <fstream>
#endif

#include "barebones/main.hpp"
#include "barebones/rand.hpp"
#include "barebones/xml.hpp"
#include "barebones/g3d.hpp"
#include "external/ogl-math/glm/gtx/transform.hpp"

#include "paths.hpp"

int DEBUG_LEVEL = 0; // default to 0 for release

void create_shaders(main_t& main); // shaders.cpp

const char* const main_t::game_name = "Ludum Dare Mini 31 - Fear"; // window titles etc

struct rect_t {
	rect_t() {}
	rect_t(const glm::vec2& a,const glm::vec2& b): bl(a), tr(b) {}
	glm::vec2 bl, tr;
	bool intersects(const rect_t& other) const {
		return (bl.x < other.tr.x && tr.x > other.bl.x &&
			bl.y < other.tr.y && tr.y > other.bl.y);
	}
	bool contains(const glm::vec2& p) const {
		return (p.x >= bl.x && p.y >= bl.y && p.x < tr.x && p.y < tr.y);
	}
	glm::vec2 centre() const {
		return glm::vec2((bl.x+tr.x)/2,(bl.y+tr.y)/2);
	}
	void normalise() {
		const float minx(std::min(bl.x,tr.x)), miny(std::min(bl.y,tr.y)),
			maxx(std::max(bl.x,tr.x)), maxy(std::max(bl.y,tr.y));
		bl = glm::vec2(minx,miny);
		tr = glm::vec2(maxx,maxy);
	}
	rect_t shrink(const glm::vec2& margin) const {
		rect_t ret(bl+margin,tr-margin);
		ret.normalise(); // so we don't care if it turns inside out
		return ret;
	}
	rect_t grow(const glm::vec2& margin) const {
		rect_t ret(bl-margin,tr+margin);
		ret.normalise(); // so we don't care if it turns inside out
		return ret;
	}
	void draw(main_t& main,const glm::mat4& mvp,glm::vec4 colour);
};

class main_game_t: public main_t, private main_t::file_io_t {
public:
	main_game_t(void* platform_ptr): main_t(platform_ptr),
		mode(MODE_LOAD), active_model(NULL), active_object(NULL), player(NULL), 
		bridge_broken(false), won(false), exit(false), exited(false),
		mouse_down(false) {}
	void init();
	bool tick();
	void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data);
	// debug just print state
	bool on_key_down(short code);
	bool on_key_up(short code);
	bool on_mouse_down(int x,int y,mouse_button_t button);
	bool on_mouse_up(int x,int y,mouse_button_t button);
	rand_t rand;
	struct artwork_t;
private:
	friend struct artwork_t;
	void on_ready(artwork_t* artwork);
	bool is_ready() const;
	void save();
	void play();
	void play_tick(float step);
	artwork_t* load_asset(xml_walker_t& xml,artwork_t* parent=NULL);
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
		MODE_SPLASH,
		MODE_PLAY,
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
	object_t* player, *balrog;
	rect_t screen;
	struct hot_t: public rect_t {
		hot_t(): type(BAD) {}
		enum {
			STOP,
			BRIDGE,
			SPECIAL,
			BAD
		} type;
		object_t* special;
	};
	typedef std::vector<hot_t> hots_t;
	hots_t hots;
	hot_t new_hot, active_hot;
	bool bridge_broken, won, exit, exited;
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
		CLS_SPECIAL = 60,
		CLS_FRONT = 21,
	};
	virtual ~artwork_t() {}
	main_game_t& game;
	artwork_t* const parent;
	const std::string id, name;
	const class_t cls;
	const glm::mat4 tx;
	const glm::vec3 anchor;
	const float scale_factor, speed, animation_length;
	const float attack_points, health_points, attack_range, defend_range;
	virtual void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour = glm::vec4(1,1,1,1)) = 0;
	virtual void draw(const rect_t& rect,const glm::mat4& projection,const glm::vec4& colour = glm::vec4(1,1,1,1)) {} // for splash etc
	virtual artwork_t* get_child(const std::string& id) = 0;
	virtual void bounds(glm::vec3& min,glm::vec3& max) = 0;
	rect_t rect() {
		if(!_bounds && is_ready()) {
			glm::vec3 min, max;
			bounds(min,max);
			const glm::vec4
				mn = tx*glm::vec4(min,1.),
				mx = tx*glm::vec4(max,1.);
			_rect.bl = glm::vec2(mn.x-anchor.x,mn.y-anchor.y);
			_rect.tr = glm::vec2(mx.x-anchor.x,mx.y-anchor.y);
			const artwork_t* p = parent;
			while(p) {
				_rect.bl.x -= p->anchor.x;
				_rect.tr.x -= p->anchor.x;
				_rect.tr.y -= p->anchor.y;
				_rect.bl.y -= p->anchor.y;
				p = p->parent;
			}
			_rect.normalise();
			_bounds = true;
		}
		return _rect;
	}
	virtual void save(std::stringstream& xml) = 0;
	virtual bool is_ready() = 0;
	float effective_animation_length() const { return animation_length? animation_length: 2; }
protected:
	void on_ready(bool ok) {
		if(!ok) data_error("failed to load " << id);
		game.on_ready(this);
	}
	artwork_t(main_game_t& g,artwork_t* p,const std::string& id_,const std::string& n,class_t c,float sf,
		const glm::vec3& a,float sp,float al,float ap,float hp,float ar,float dr):
		game(g), parent(p), id(id_), name(n), cls(c),
		tx(glm::scale(glm::vec3(sf,sf,sf))),
		anchor(a),
		scale_factor(sf), speed(sp), animation_length(al),
		attack_points(ap), health_points(hp), attack_range(ar), defend_range(dr), _bounds(false) {}
private:
	rect_t _rect;
	bool _bounds;
};

struct artwork_splash_t: public main_game_t::artwork_t, private main_t::texture_load_t {
	artwork_splash_t(main_game_t& main,artwork_t* parent,const std::string& id_,const std::string& p,class_t c,float sf,float sp,
		const glm::vec3& a,float al,float attack_points,float health_points,float attack_range,float defend_range):
		artwork_t(main,parent,id_,p,c,sf,a,sp,al,attack_points,health_points,attack_range,defend_range),
		path(p), texture(0) {
			main.load_texture(path,this,0);
		}
	const std::string path;
	GLuint texture;
	void on_texture_loaded(const std::string& name,GLuint handle,intptr_t data) { 
		texture = handle;
		if(!is_ready())
			data_error("could not load splash " << id << ':' << path);
	}
	bool is_ready() { return texture; }
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour) {}
	void draw(const rect_t& rect,const glm::mat4& projection,const glm::vec4& colour) {
		if(!is_ready()) return;
		const glm::vec4 bl = projection * glm::vec4(rect.bl,0,1), tr = projection * glm::vec4(rect.tr,0,1);
		const float x1 = bl.x, y1 = bl.y, x2 = tr.x, y2 = tr.y;
		const GLfloat data[4*2+4*2] = {
			x1,y1,x2,y1,x1,y2,x2,y2,
			0,1,1,1,0,0,1,0};
		GLuint program = game.get_shared_program("splash"),
			uniform_colour = game.get_uniform_loc(program,"COLOUR",GL_FLOAT_VEC4),
			attrib_vertex = game.get_attribute_loc(program,"VERTEX",GL_FLOAT_VEC2),
			attrib_tex = game.get_attribute_loc(program,"TEX_COORD_0",GL_FLOAT_VEC2),
			vbo;
		glUseProgram(program);
		glUniform4fv(uniform_colour,1,glm::value_ptr(colour));
		glCheck();
		glGenBuffers(1,&vbo);
		glBindBuffer(GL_ARRAY_BUFFER,vbo);
		glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);
		glEnableVertexAttribArray(attrib_vertex);
		glEnableVertexAttribArray(attrib_tex);
		glVertexAttribPointer(attrib_vertex,2,GL_FLOAT,GL_FALSE,0,0);
		glVertexAttribPointer(attrib_tex,2,GL_FLOAT,GL_FALSE,0,(GLvoid*)(4*2*sizeof(GLfloat)));
		glBindTexture(GL_TEXTURE_2D,texture);
		glCheck();
		glDrawArrays(GL_TRIANGLE_STRIP,0,4);
		glDeleteBuffers(1,&vbo);
		glDisableVertexAttribArray(attrib_vertex);
		glDisableVertexAttribArray(attrib_tex);
		glBindTexture(GL_TEXTURE_2D,0);
		glBindBuffer(GL_ARRAY_BUFFER,0);
		glUseProgram(0);
		glCheck();
	}
	artwork_t* get_child(const std::string& id) { return this; }
	void bounds(glm::vec3& min,glm::vec3& max) {
		min = glm::vec3(-10000,-10000,-10000);
		max = glm::vec3(10000,100000,100000);
	}
	void save(std::stringstream& xml) {
		std::string indent = "\t\t";
		for(const artwork_t* p=parent; p; p = p->parent)
			indent += "\t";
		xml << indent << "<asset id=\"" << id << "\" type=\"splash\" class=\"" <<
			(cls == artwork_t::CLS_BACK?"back":
			cls == artwork_t::CLS_PLAYER?"player":
			cls == artwork_t::CLS_SPECIAL?"special":
			cls == artwork_t::CLS_FRONT?"front":
			cls == artwork_t::CLS_MONSTER?"monster":
				"BUG") << "\" path=\"" << path << "\"/>\n";
	}
};

struct artwork_set_t: public main_game_t::artwork_t {
	artwork_set_t(main_game_t& main,artwork_t* parent,const std::string& id_,class_t c,float sf,float sp,
		const glm::vec3& a,float al,float attack_points,float health_points,float attack_range,float defend_range):
		artwork_t(main,parent,id_,id_,c,sf,a,sp,al,attack_points,health_points,attack_range,defend_range) {}
	~artwork_set_t() {
		for(artworks_t::iterator i=artwork.begin(); i!=artwork.end(); i++)
			delete *i;
	}
	typedef std::vector<artwork_t*> artworks_t;
	artworks_t artwork;
	artwork_t* get_child(const std::string& id) {
		artworks_t match; // no coffee-fueled reservoir sampling
		for(artworks_t::iterator i=artwork.begin(); i!=artwork.end(); i++)
			if((*i)->id == id)
				match.push_back(*i);
		if(match.size()) {
			const size_t i = game.rand.rand(match.size());
			return match.at(i);
		}
		std::cout << "set " << this->id << " has no child " << id << std::endl;
		return artwork.front(); // default
	}
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour) {
		artwork.front()->draw(time,projection,modelview,light0,colour);
	}
	void bounds(glm::vec3& min, glm::vec3& max) {
		for(artworks_t::iterator i=artwork.begin(); i!=artwork.end(); i++) {
			glm::vec3 mn, mx;
			(*i)->bounds(mn,mx);
			min.x = std::min(mn.x,min.x); min.y = std::min(mn.y,min.y);
			max.x = std::max(mx.x,max.x); max.y = std::max(mx.y,max.y);
		}
	}
	void save(std::stringstream& xml) {
		std::string indent = "\t\t";
		for(const artwork_t* p=parent; p; p = p->parent)
			indent += "\t";
		xml << indent << "<asset id=\"" << id << "\" type=\"set\" class=\"" <<
			(cls == artwork_t::CLS_BACK?"back":
			cls == artwork_t::CLS_PLAYER?"player":
			cls == artwork_t::CLS_SPECIAL?"special":
			cls == artwork_t::CLS_FRONT?"front":
			cls == artwork_t::CLS_MONSTER?"monster":
				"BUG") << "\" ";
		const float sf = (parent && parent->scale_factor?scale_factor/parent->scale_factor:scale_factor);
		if(sf !=1)
			xml << " scale_factor=\"" << sf << "\"";
		const float sp = (parent && parent->speed?speed/parent->speed:speed);
		if(sp != 1)
			xml << " speed=\"" << sp << "\"";
		if(attack_points)
			xml << " attack_points=\"" << attack_points << "\"";
		if(health_points)
			xml << " health_points=\"" << health_points << "\"";
		if(attack_range)
			xml << " attack_range=\"" << attack_range << "\"";
		if(defend_range)
			xml << " defend_range=\"" << defend_range << "\"";
		if(anchor.x)
			xml << " anchor_x=\"" << anchor.x << "\"";
		if(anchor.y)
			xml << " anchor_y=\"" << anchor.y << "\"";
		if(anchor.z)
			xml << " anchor_z=\"" << anchor.z << "\"";
		xml << ">\n";
		for(artworks_t::iterator i=artwork.begin(); i!=artwork.end(); i++)
			(*i)->save(xml);
		xml << indent << "</asset>\n";
	}
	bool is_ready() {
		for(artworks_t::iterator i=artwork.begin(); i!=artwork.end(); i++)
			if(!(*i)->is_ready())
				return false;
		return true;
	}
};

struct artwork_g3d_t: public main_game_t::artwork_t, private g3d_t::loaded_t {
	artwork_g3d_t(main_game_t& main,artwork_t* parent,const std::string& id_,const std::string& p,class_t c,bool cy,float sf,float sp,
		const glm::vec3& a,float al,float attack_points,float health_points,float attack_range,float defend_range):
		artwork_t(main,parent,id_,p,c,sf,a,sp,al,attack_points,health_points,attack_range,defend_range),
		path(p), cycles(cy), g3d(main,p,this), _ready(false) {}
	const std::string path;
	const bool cycles;
	g3d_t g3d;
	void on_g3d_loaded(g3d_t& g3d,bool ok,intptr_t data) {
		if(!ok) data_error("failed to load " << path);
		_ready = true;
		artwork_t::on_ready(this);
	}
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light0,const glm::vec4& colour) {
		const float anim_len = (animation_length>0?animation_length:2);
		const float t = fmod(time/anim_len,1);
		g3d.draw(t,projection,modelview,light0,cycles,colour);
	}
	artwork_t* get_child(const std::string& id) { return this; }
	void bounds(glm::vec3& min,glm::vec3& max) { return g3d.bounds(min,max); }
	void save(std::stringstream& xml) {
		std::string indent = "\t\t";
		for(const artwork_t* p=parent; p; p = p->parent)
			indent += "\t";
		xml << indent << "<asset id=\"" << id << "\" type=\"g3d\" class=\"" <<
			(cls == artwork_t::CLS_BACK?"back":
			cls == artwork_t::CLS_PLAYER?"player":
			cls == artwork_t::CLS_SPECIAL?"special":
			cls == artwork_t::CLS_FRONT?"front":
			cls == artwork_t::CLS_MONSTER?"monster":
				"BUG") <<
			"\" path=\"" << path << "\"";
		if(!cycles)
			xml << " cycles=\"false\"";
		const float sf = (parent && parent->scale_factor?scale_factor/parent->scale_factor:scale_factor);
		if(sf !=1)
			xml << " scale_factor=\"" << sf << "\"";
		const float sp = (parent && parent->speed?speed/parent->speed:speed);
		if(sp != 1)
			xml << " speed=\"" << sp << "\"";
		if(attack_points)
			xml << " attack_points=\"" << attack_points << "\"";
		if(health_points)
			xml << " health_points=\"" << health_points << "\"";
		if(attack_range)
			xml << " attack_range=\"" << attack_range << "\"";
		if(defend_range)
			xml << " defend_range=\"" << defend_range << "\"";
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
	bool is_ready() { return _ready; }
	bool _ready;
};

main_game_t::artwork_t* main_game_t::load_asset(xml_walker_t& xml,artwork_t* parent) {
	const float sf = parent? parent->scale_factor: 1;
	const float sp = parent? parent->speed: 1;
	const std::string id = xml.value_string("id"),
		type = xml.value_string("type"), 
		scls = xml.value_string("class");
	main_game_t::artwork_t::class_t cls;
	if(scls == "back") cls = main_game_t::artwork_t::CLS_BACK;
	else if(scls=="monster") cls = main_game_t::artwork_t::CLS_MONSTER;
	else if(scls=="player") cls = main_game_t::artwork_t::CLS_PLAYER;
	else if(scls=="special") cls = main_game_t::artwork_t::CLS_SPECIAL;
	else if(scls=="front") cls = main_game_t::artwork_t::CLS_FRONT;
	else data_error(scls << " is not a supported artwork class");
	const glm::vec3 anchor(
		xml.has_key("anchor_x")? xml.value_float("anchor_x"):0,
		xml.has_key("anchor_y")? xml.value_float("anchor_y"):0,
		xml.has_key("anchor_z")? xml.value_float("anchor_z"):0);
	const float scaler = (xml.has_key("scale_factor")? xml.value_float("scale_factor"):1)*sf;
	const float speed = (xml.has_key("speed")? xml.value_float("speed"):1) * sp;
	const float animation_length = xml.has_key("animation_length")? xml.value_float("animation_length"): 0;
	const float attack_points = xml.has_key("attack_points")? xml.value_float("attack_points"): 0;
	const float health_points = xml.has_key("health_points")? xml.value_float("health_points"): 0;
	const float attack_range = xml.has_key("attack_range")? xml.value_float("attack_range"): 0;
	const float defend_range = xml.has_key("defend_range")? xml.value_float("defend_range"): 0;
	if(type == "g3d") {
		const std::string path = xml.value_string("path");
		const bool cycles = xml.has_key("cycles")? xml.value_bool("cycles"): true;
		std::cout << "loading G3D " << path << std::endl;
		return new artwork_g3d_t(*this,parent,id,path,cls,cycles,scaler,speed,anchor,animation_length,attack_points,health_points,attack_range,defend_range);
	} else if(type == "splash") {
		const std::string path = xml.value_string("path");
		std::cout << "loading splash " << path << std::endl;
		return new artwork_splash_t(*this,parent,id,path,cls,scaler,speed,anchor,animation_length,attack_points,health_points,attack_range,defend_range);
	} else if(type == "set") {
		artwork_set_t* set = new artwork_set_t(*this,parent,id,cls,scaler,speed,anchor,animation_length,attack_points,health_points,attack_range,defend_range);
		for(int i=0; xml.get_child("asset",i); i++, xml.up())
			set->artwork.push_back(load_asset(xml,set));
		return set;
	} else
		data_error("unsupported artwork type "<<type);
}

struct main_game_t::object_t {
	object_t(artwork_t& a,const glm::vec2& p):
		artwork(a), pos(p), state(WALKING), attacking(false), defending(false), waiting(true),
		has_pickaxe(false), health_points(a.health_points), bury(false) {
			dir[WALKING] = dir[JUMPING] = EDITOR;
			action[WALKING] = action[JUMPING] = "idle";
			active_artwork[WALKING] = active_artwork[JUMPING] = artwork.get_child("idle");
			speed[WALKING] = speed[JUMPING] = 0;
			animation_start[WALKING] = animation_start[JUMPING] = artwork.game.now_secs();
		}
	artwork_t& artwork;
	glm::vec2 pos;
	enum state_t {
		WALKING,
		JUMPING,
		STATE_LAST
	} state;
	enum {
		LEFT = -1,
		EDITOR = 0,
		RIGHT = 1
	} dir[STATE_LAST];
	bool attacking, defending;
	bool waiting, has_pickaxe;
	double jump_gravity, jump_energy;
	float health_points;
	artwork_t* active_artwork[STATE_LAST];
	double animation_start[STATE_LAST], speed[STATE_LAST];
	std::string action[STATE_LAST];
	void set_action(state_t state,const std::string& action) {
		if(action == this->action[state]) return;
		this->action[state] = action;
		active_artwork[state] = artwork.get_child(action);
		animation_start[state] = artwork.game.now_secs();
	}
	void set_state(state_t state) {
		if(state == this->state) return;
		this->state = state;
		animation_start[state] = artwork.game.now_secs();
	}
	void draw(float time,const glm::mat4& projection,const glm::vec3& light0,const glm::vec4& colour = glm::vec4(1,1,1,1)) {
		if(bury) return;
		time -= animation_start[state];
		if(time > active_artwork[state]->effective_animation_length()) { // time to change it then
			if(is_dead()) {
				bury = true;
				return;
			} else if(action[state] == "collapse" || action[state] == "collapse_boss") { // bridge
				set_action(state,artwork.game.won?"collapsed_open":"collapsed");
				artwork.game.exit = true;
			} else if(action[state] != "collapsed_open" && action[state] != "collapsed") {
				active_artwork[state] = artwork.get_child(action[state]);
				animation_start[state] = artwork.game.now_secs();
			}
		}
		active_artwork[state]->draw(time,projection,tx(),light0,colour);		
	}
	glm::mat4 tx() {
		glm::mat4 tx(glm::translate(glm::vec3(pos.x-artwork.anchor.x,pos.y-artwork.anchor.y,-artwork.cls))*artwork.tx);
		if(dir[WALKING] == LEFT) // WALKING defines facing, even if completing a jump in another direction
			tx *= glm::rotate(270.0f,glm::vec3(0,1,0));
		else if(dir[WALKING] == RIGHT)
			tx *= glm::rotate(90.0f,glm::vec3(0,1,0));
		return tx;
	}
	bool is_visible(const rect_t& screen) const {
		return screen.intersects(effective_rect());
	}
	rect_t effective_rect() const {
		const glm::vec2 anchor(artwork.anchor.x,pos.y-artwork.anchor.y);
		rect_t rect = active_artwork[state]->rect();
		return rect_t(rect.bl+pos,rect.tr+pos);
	}
	rect_t attack_rect() const {
		return effective_rect().grow(glm::vec2(artwork.attack_range,0));
	}
	rect_t defend_rect() const {
		return effective_rect().grow(glm::vec2(artwork.defend_range,0));
	}
	bool is_dead() const { return (health_points <= 0) && artwork.health_points; }
	bool bury;
};

void rect_t::draw(main_t& main,const glm::mat4& mvp,glm::vec4 colour) {
	const GLfloat data[4*2] = {
		bl.x,bl.y,
		tr.x,bl.y,
		tr.x,tr.y,
		bl.x,tr.y};
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
	glDisableVertexAttribArray(attrib_vertex);
	glBindBuffer(GL_ARRAY_BUFFER,0);
	glUseProgram(0);
	glCheck();
}

void main_game_t::init() {
	create_shaders(*this);
	glClearColor(0,0,0,1);
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
		xml.check("game");
		if(xml.has_key("debug_level"))
			DEBUG_LEVEL = xml.value_int("debug_level");
		xml.get_child("artwork");
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
			if(artwork[asset]->cls == artwork_t::CLS_SPECIAL) {
				new_hot.type = hot_t::SPECIAL;
				new_hot.special = objects.back();
				rect_t r = new_hot.special->effective_rect();
				new_hot.bl = r.bl;
				new_hot.tr = r.tr;
				new_hot.normalise();
				hots.push_back(new_hot);
			}
		}
		for(int i=0; xml.get_child("hot",i); i++, xml.up()) {
			new_hot.bl.x = xml.value_float("x1");
			new_hot.bl.y = xml.value_float("y1");
			new_hot.tr.x = xml.value_float("x2");
			new_hot.tr.y = xml.value_float("y2");
			new_hot.normalise();
			const std::string type = xml.value_string("type");
			if(type == "stop") new_hot.type = hot_t::STOP;
			else if(type == "bridge") new_hot.type = hot_t::BRIDGE;
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
		
		if(!DEBUG_LEVEL) // with debug-level, we're in EDIT mode
			play();
	}
}

bool main_game_t::tick() {
	static double last_tick = now_secs();
	const double now = now_secs(), since_last = now-last_tick;
	if(mode == MODE_SPLASH) {
		artwork["SPLASH"]->draw(rect_t(glm::vec2(-1,-1),glm::vec2(1,1)),glm::mat4(),glm::vec4(1,1,1,1));
		return true;
	} else if(mode == MODE_PLAY) {
		if(exited) {
			artwork["WIN"]->draw(rect_t(glm::vec2(-1,-1),glm::vec2(1,1)),glm::mat4(),glm::vec4(1,1,1,1));
			return true;
		}
		if(player->bury) {
			artwork["LOSE"]->draw(rect_t(glm::vec2(-1,-1),glm::vec2(1,1)),glm::mat4(),glm::vec4(1,1,1,1));
			return true;
		}
		play_tick(since_last);
		if(won && !balrog->bury)
			screen_centre = balrog->pos - balrog->artwork.rect().centre();
		else
			screen_centre = player->pos - player->artwork.rect().centre();
	} else
		screen_centre += pan_rate * glm::vec2(since_last,since_last);
	screen.bl = glm::vec2(screen_centre.x-width/2,screen_centre.y-height/2);
	screen.tr = glm::vec2(screen_centre.x+width/2,screen_centre.y+height/2);
	const glm::mat4 projection(glm::ortho<float>(
		screen.bl.x,screen.tr.x, // 0,0 is screen centre
		screen.bl.y,screen.tr.y, // y increases upwards
		1,300));
	const glm::vec3 light0(10,10,10);
	// show all the objects
	objects_t reap;
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++) {
		if((*i)->is_visible(screen))
			(*i)->draw(now,projection,light0);
		if(DEBUG_LEVEL) {
			if((*i)->defending)
				(*i)->defend_rect().draw(*this,projection,glm::vec4(1,1,1,.5));
			if((*i)->attacking)
				(*i)->attack_rect().draw(*this,projection,glm::vec4(1,0,0,.5));
		}
		if((mode == MODE_PLAY) && (*i)->bury && *i!=player && *i!=balrog)
			reap.push_back(*i);
	}
	for(objects_t::iterator i=reap.begin(); i!=reap.end(); i++) {
		objects.erase(std::find(objects.begin(),objects.end(),*i));
		delete *i;
	}
	if(mode != MODE_PLAY) {
		// show active model on top for editing
		if((mode == MODE_PLACE_OBJECT) && active_model && mouse_down) {
			active_model->draw(now,projection,
				glm::translate(glm::vec3(screen_centre.x+mouse_x-width/2,screen_centre.y-mouse_y+height/2,-50))*
					active_model->tx,
				light0,glm::vec4(1,.6,.6,.6));
		}
		if((mode == MODE_HOT) && draw_hot)
			new_hot.draw(*this,projection,glm::vec4(1,1,0,1));
	}
	if(mode != MODE_PLAY || DEBUG_LEVEL) {
		for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++)
			i->draw(*this,projection,glm::vec4(0,1,0,1));
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
	if(player->is_dead()) return;
	// move main player
	{
		glm::vec2 move(player->speed[player->state] * step * player->dir[player->state],0),
			player_pos(glm::vec2(player->artwork.anchor.x,player->artwork.anchor.y)+player->pos);
		float floor_y;
		if(floor->y_at(player_pos,floor_y,true)) {
			if(player->state == object_t::JUMPING) {
				player->jump_gravity += 4.0*step;
				const double jump_up = player->jump_energy*step - player->jump_gravity;
				if(jump_up+player_pos.y < floor_y) {
					move.y = floor_y-player_pos.y;
					player->set_state(object_t::WALKING);
					if(keys()[KEY_UP]) // still pressed? jump again
						on_key_down(KEY_UP);
				} else
					move.y = jump_up;
			}
			if(player->state == object_t::WALKING) {
				player->attacking = !player->speed[player->state] && keys()[' '];
				if(player->attacking)
					player->set_action(player->state,player->has_pickaxe?"pickaxe":"attack");
				else if(player->speed[player->state])
					player->set_action(player->state,"run");
				else
					player->set_action(player->state,"idle");
				move.y = floor_y-player_pos.y;
			}
		} else // else path says stop; play a bump sound?
			move.x = 0;
		glm::vec2 new_pos = player->pos + move;
		for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++)
			if(i->contains(new_pos))
				switch(i->type) {
				case hot_t::SPECIAL:
					if(i->special) {
						if(i->special->artwork.id == "PICKAXE") {
							i->special->bury = true;
							player->has_pickaxe = true;
						} else
							data_error("unsupported special " << i->special->artwork.id);
						i->special = NULL; // only trigger once
					}
					break;
				case hot_t::BRIDGE:
					if(bridge_broken) {
						std::cout << "BRIDGE is BROKEN so you die" << std::endl;
						player->health_points = -1;
					} else if(player->attacking && player->has_pickaxe) {
						std::cout << "YOU SWING AXE to BREAK BRIDGE" << std::endl;
						bridge_broken = true;
						rect_t shrink = i->shrink(glm::vec2(60,0)); // make it so you can survive on the edge
						i->bl = shrink.bl; i->tr = shrink.tr;
						bool balrog_dies = false;
						object_t* chasm = NULL;
						for(objects_t::iterator o=objects.begin(); o!=objects.end(); o++)
							if((*o)->artwork.id == "CHASM") {
								chasm = *o;
							} else if((*o)->defend_rect().intersects(shrink)) {
								std::cout << "BRIDGE is BROKEN so " << (*o)->artwork.id << " dies" << std::endl;
								(*o)->health_points = -1;
								balrog_dies |= (*o == balrog);
								(*o)->set_action((*o)->state,"pitfall");
							}
						chasm->set_action(chasm->state,balrog_dies?"collapse_boss":"collapse");
						won = balrog_dies && !player->is_dead();
						return;
					}
					break;
				case hot_t::STOP:
					if(exit)
						exited = true; // any stop zone will work, so you can escape from the wrong side of the bridge
					move.x = 0; // stop lateral movement in that direction
					break;
				default:;
				}
		player->pos += move;
	}
	// move all monsters
	const rect_t player_defend(player->defend_rect()),
		player_attack(player->attack_rect());
	player->defending = false;
	for(objects_t::iterator i=objects.begin(); i!=objects.end(); i++)
		if((*i)->artwork.cls == artwork_t::CLS_MONSTER) {
			object_t& monster = **i;
			if(monster.is_dead()) continue;
			if(monster.waiting) {
				if(monster.is_visible(screen)) {
					monster.waiting = false;
					std::cout << "monster " << monster.artwork.id << " activated" << std::endl;
				}
			} else {
				const rect_t monster_defend(monster.defend_rect()),
					monster_attack(monster.attack_rect());
				bool run = false;
				monster.attacking = false;
				monster.defending = false;
				if(monster.pos.x < player->pos.x) {
					run = true;
					monster.dir[monster.state] = object_t::RIGHT;
				} else if(monster.pos.x > player->pos.x) {
					run = true;
					monster.dir[monster.state] = object_t::LEFT;
				}
				if(monster_attack.intersects(player_defend)) {
					run = false;
					monster.attacking = true;
					monster.set_action(monster.state,"attack");
					player->defending = true;
					player->health_points -= monster.artwork.attack_points * step;
				} else if(run) {
					monster.set_action(monster.state,"run");
					const float speed = monster.active_artwork[monster.state]->speed;
					glm::vec2 move = floor->route(monster.pos,speed*step,player->pos,true);
					for(hots_t::iterator i=hots.begin(); i!=hots.end(); i++)
						if(bridge_broken && i->type == hot_t::BRIDGE && i->contains(move) &&
							monster.artwork.id != "bat1") {
							move.x = 0; // cancel
							break;
						}
					monster.pos = move;
				} else
					monster.set_action(monster.state,"idle");
				if(player->attacking &&
					((player->pos.x > monster.pos.x) == (player->dir[object_t::WALKING] == object_t::LEFT)) &&
					player_attack.intersects(monster_defend)) {
					monster.defending = true;
					monster.health_points -= player->artwork.attack_points * step;
					if(monster.is_dead())
						monster.set_action(player->state,"die");
				}
			}
		}
	if(player->is_dead()) {
		std::cout << "player dies!" << std::endl;
		player->set_action(player->state,"die");
	}
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
		if(i->type == hot_t::SPECIAL) continue; // not in the xml
		xml << "\t\t<hot x1=\"" << i->bl.x << "\" y1=\"" << i->bl.y << "\" x2=\"" << i->tr.x << "\" y2=\"" << i->tr.y << "\" type=\"";
		if(i->type == hot_t::STOP) xml << "stop";
		else if(i->type == hot_t::BRIDGE) xml << "bridge";
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
	player = balrog = NULL;
	for(objects_t::iterator o=objects.begin(); o!=objects.end(); o++) {
		(*o)->set_state(object_t::WALKING);
		if((*o)->artwork.cls == artwork_t::CLS_PLAYER) {
			if(player) {
				std::cerr << "There is more than one player!  Cannot play." << std::endl;
				player = NULL;
				return;
			} else
				player = *o;
		} else if((*o)->artwork.id == "BALROG")
			balrog = *o;
	}
	if(!player) {
		std::cerr << "There is no player!  Cannot play." << std::endl;
		return;
	}
	if(!balrog) {
		std::cerr << "There is no balrog!  Cannot play." << std::endl;
		return;
	}
	if(!player->artwork.get_child("run")->speed) {
		std::cerr << "The player cannot move!  Add a speed= parameter to the xml!" << std::endl;
		return;
	}
	mode = MODE_SPLASH;
	pan_rate = glm::vec2(0,0);
	glClearColor(0,0,0,1);
	std::cout << "Playing game!" << std::endl;
}

bool main_game_t::on_key_down(short code) {
	if(mode == MODE_SPLASH) {
		mode = MODE_PLAY;
		return true;
	} else if(mode == MODE_PLAY) {
		if(player->is_dead()) return true;
		switch(code) {
		case KEY_UP:
			if(player->state == object_t::WALKING) {
				player->jump_gravity = 0;
				player->jump_energy = 160;
				player->dir[object_t::JUMPING] = player->dir[object_t::WALKING];
				player->speed[object_t::JUMPING] = player->speed[object_t::WALKING];
				player->set_state(object_t::JUMPING);
				player->attacking = keys()[' '];
				player->set_action(player->state,player->attacking?"jump_attack":"jump");
			}
			break;
		case KEY_LEFT:
			if(keys()[KEY_RIGHT]) break;
			player->set_action(object_t::WALKING,"run");
			player->speed[object_t::WALKING] = player->active_artwork[object_t::WALKING]->speed;
			player->dir[object_t::WALKING] = object_t::LEFT;
			break;
		case KEY_RIGHT:
			if(keys()[KEY_LEFT]) break;
			player->set_action(object_t::WALKING,"run");
			player->speed[object_t::WALKING] = player->active_artwork[object_t::WALKING]->speed;
			player->dir[object_t::WALKING] = object_t::RIGHT;
			break;
		case ' ':
			if(player->state == object_t::JUMPING) {
				player->attacking = true;
				player->set_action(object_t::JUMPING,"jump_attack");
			} else {
				assert(player->state == object_t::WALKING);
				if(player->speed[object_t::WALKING] == 0) { // no attack whilst running
					player->attacking = true;
					player->set_action(object_t::WALKING,player->has_pickaxe?"pickaxe":"attack");
				}
			}
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
	case 'h': case 'H': mode = MODE_HOT; draw_hot = false; std::cout << "HOT ZONE MODE" << std::endl; return true;
	case 'f': case 'F': mode = MODE_FLOOR; std::cout << "FLOOR MODE" << std::endl; return true;
	case 'c': case 'C': mode = MODE_CEILING; std::cout << "CEILING MODE" << std::endl; return true;
	case 'e': case 'E':
		active_object = NULL;
		mode = MODE_EDIT_OBJECT;
		std::cout << "OBJECT EDIT MODE " << (active_object?active_object->artwork.id:"<no selection>") << std::endl; 
		return true;
	case 'o': case 'O':
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
	case 'p': case 'P':
		play();
		return true;
	default:
		switch(mode) {
		case MODE_EDIT_OBJECT:
			return false;
		case MODE_FLOOR:
			return floor->on_key_down(code,keys(),mouse());
		case MODE_CEILING:
			return ceiling->on_key_down(code,keys(),mouse());
		default: return false;
		}		
	}
}

bool main_game_t::on_key_up(short code) {
	if(mode == MODE_PLAY) {
		if(player->is_dead()) return true;
		switch(code) {
		case KEY_UP:
			player->jump_energy *= 0.6; // stop them jumping full-apogee
			break;
		case KEY_LEFT:
			if(keys()[KEY_RIGHT])
				on_key_down(KEY_RIGHT);
			else {
				player->set_action(object_t::WALKING,"idle");
				player->speed[object_t::WALKING] = 0;
				player->animation_start[object_t::WALKING] = now_secs();
			}
			break;
		case KEY_RIGHT:
			if(keys()[KEY_LEFT])
				on_key_down(KEY_LEFT);
			else {
				player->set_action(object_t::WALKING,"idle");
				player->speed[object_t::WALKING] = 0;
				player->animation_start[object_t::WALKING] = now_secs();
			}
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
	case 's': case 'S': if(keys().none()) save(); return true;
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
			return floor->on_key_up(code,keys(),mouse());
		case MODE_CEILING:
			return ceiling->on_key_up(code,keys(),mouse());
		case MODE_HOT:
			if(mouse_down || !draw_hot) return false;
			switch(code) {
			case 'x': case 'X':
				new_hot.type = hot_t::STOP;
				std::cout << "HOT is STOP" << std::endl;
				break;
			case 'b': case 'B':
				new_hot.type = hot_t::BRIDGE;
				std::cout << "HOT is BRIDGE" << std::endl;
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

bool main_game_t::on_mouse_down(int x,int y,mouse_button_t button) {
	if(button != MOUSE_LEFT)
		return false;
	mouse_down = true;
	mouse_x = x;
	mouse_y = y;
	const int mapped_x = screen_centre.x+mouse_x-width/2, mapped_y = screen_centre.y-mouse_y+height/2;
	switch(mode) {
	case MODE_SPLASH:
		mode = MODE_PLAY;
		return true;
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
		floor->on_mouse_down(mapped_x,mapped_y,button,keys(),mouse());
		break;
	case MODE_CEILING:
		ceiling->on_mouse_down(mapped_x,mapped_y,button,keys(),mouse());
		break;
	case MODE_HOT:
		if(!draw_hot)
			new_hot.bl = glm::vec2(mapped_x,mapped_y);
		draw_hot = true;
		new_hot.tr = glm::vec2(mapped_x,mapped_y);
		break;
	default:;
	}
	return true;
}

bool main_game_t::on_mouse_up(int x,int y,mouse_button_t button) {
	if(button != MOUSE_LEFT)
		return false;
	mouse_down = false;
	mouse_x = x;
	mouse_y = y;
	const int mapped_x = screen_centre.x+mouse_x-width/2, mapped_y = screen_centre.y-mouse_y+height/2;
	switch(mode) {
	case MODE_LOAD:
		break;
	case MODE_FLOOR:
		floor->on_mouse_up(mapped_x,mapped_y,button,keys(),mouse());
		break;
	case MODE_CEILING:
		ceiling->on_mouse_up(mapped_x,mapped_y,button,keys(),mouse());
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
