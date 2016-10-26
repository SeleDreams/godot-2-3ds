/*************************************************************************/
/*  rasterizer_citro3d.cpp                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifdef _3DS

#include "rasterizer_citro3d.h"
#include "globals.h"
#include "os/os.h"
#include "util.h"

// Built-in shaders generated in byte array headers
#include "shaders/2d.h"
#include "shaders/3d.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_XY))

/* TEXTURE API */

// #define print(...) printf(__VA_ARGS__)
#define print(...)

RID RasterizerCitro3d::texture_create()
{
	Texture *texture = memnew(Texture);
	ERR_FAIL_COND_V(!texture,RID());
	return texture_owner.make_rid( texture );
}

void RasterizerCitro3d::texture_allocate(RID p_texture,int p_width, int p_height,Image::Format p_format,uint32_t p_flags)
{
	u32 w = next_pow2(p_width);
	u32 h = next_pow2(p_height);
// 	print("texture_allocate %u %u %u\n", w, h, p_texture.get_id());
	Texture *texture = texture_owner.get( p_texture );
	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(w > 1024 || h > 1024);
	ERR_FAIL_COND(!C3D_TexInit(&texture->tex, w, h, GPU_RGBA8));
	texture->width=p_width;
	texture->height=p_height;
	texture->format=p_format;
	texture->flags=p_flags;
}

void RasterizerCitro3d::texture_set_data(RID p_texture,const Image& p_image,VS::CubeMapSide p_cube_side)
{
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND(!texture);
	ERR_FAIL_COND(texture->format != p_image.get_format() );

	Image &image = texture->image[p_cube_side];
	image = p_image;
	image.convert(Image::FORMAT_RGBA);
	ERR_FAIL_COND(image.get_format() != Image::FORMAT_RGBA);
	
	DVector<uint8_t> d = image.get_data();
	const u32* imagedata = reinterpret_cast<const u32*>(d.read().ptr());

	texture_tile_sw(&texture->tex, imagedata, texture->width, texture->height);

// 	texture->image[p_cube_side]=image;
}

Image RasterizerCitro3d::texture_get_data(RID p_texture,VS::CubeMapSide p_cube_side) const
{
	print("texture_get_data\n");
	Texture * texture = texture_owner.get(p_texture);

	ERR_FAIL_COND_V(!texture,Image());

	return texture->image[p_cube_side];
}

void RasterizerCitro3d::texture_set_flags(RID p_texture,uint32_t p_flags)
{
	Texture *texture = texture_owner.get( p_texture );
	ERR_FAIL_COND(!texture);
	uint32_t cube = texture->flags & VS::TEXTURE_FLAG_CUBEMAP;
	texture->flags=p_flags|cube; // can't remove a cube from being a cube
}
uint32_t RasterizerCitro3d::texture_get_flags(RID p_texture) const {
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture,0);
	return texture->flags;
}
Image::Format RasterizerCitro3d::texture_get_format(RID p_texture) const
{
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture,Image::FORMAT_GRAYSCALE);
	return texture->format;
}
uint32_t RasterizerCitro3d::texture_get_width(RID p_texture) const
{
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture,0);
	return texture->width;
}
uint32_t RasterizerCitro3d::texture_get_height(RID p_texture) const
{
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture,0);
	return texture->height;
}

bool RasterizerCitro3d::texture_has_alpha(RID p_texture) const
{
	Texture * texture = texture_owner.get(p_texture);
	ERR_FAIL_COND_V(!texture,0);
	return false;
}

void RasterizerCitro3d::texture_set_size_override(RID p_texture,int p_width, int p_height)
{
	Texture * texture = texture_owner.get(p_texture);

	ERR_FAIL_COND(!texture);

	ERR_FAIL_COND(p_width<=0 || p_width>1024);
	ERR_FAIL_COND(p_height<=0 || p_height>1024);
	//real texture size is in alloc width and height
//	texture->width=p_width;
//	texture->height=p_height;
}

void RasterizerCitro3d::texture_set_reload_hook(RID p_texture,ObjectID p_owner,const StringName& p_function) const {


}

/* SHADER API */

/* SHADER API */

RID RasterizerCitro3d::shader_create(VS::ShaderMode p_mode)
{
	print("shader_create\n");
	Shader *shader = memnew( Shader );
	shader->mode=p_mode;
	shader->fragment_line=0;
	shader->vertex_line=0;
	shader->light_line=0;
	RID rid = shader_owner.make_rid(shader);

	return rid;
}



void RasterizerCitro3d::shader_set_mode(RID p_shader,VS::ShaderMode p_mode) {

	ERR_FAIL_INDEX(p_mode,3);
	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
	shader->mode=p_mode;

}
VS::ShaderMode RasterizerCitro3d::shader_get_mode(RID p_shader) const {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader,VS::SHADER_MATERIAL);
	return shader->mode;
}

void RasterizerCitro3d::shader_set_code(RID p_shader, const String& p_vertex, const String& p_fragment,const String& p_light,int p_vertex_ofs,int p_fragment_ofs,int p_light_ofs) {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);
	shader->fragment_code=p_fragment;
	shader->vertex_code=p_vertex;
	shader->light_code=p_light;
	shader->fragment_line=p_fragment_ofs;
	shader->vertex_line=p_vertex_ofs;
	shader->light_line=p_vertex_ofs;

}


String RasterizerCitro3d::shader_get_vertex_code(RID p_shader) const {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader,String());
	return shader->vertex_code;

}

String RasterizerCitro3d::shader_get_fragment_code(RID p_shader) const {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader,String());
	return shader->fragment_code;

}

String RasterizerCitro3d::shader_get_light_code(RID p_shader) const {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND_V(!shader,String());
	return shader->light_code;

}

void RasterizerCitro3d::shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const {

	Shader *shader=shader_owner.get(p_shader);
	ERR_FAIL_COND(!shader);

}


void RasterizerCitro3d::shader_set_default_texture_param(RID p_shader, const StringName& p_name, RID p_texture)
{
	print("shader_set_default_texture_param\n");
}

RID RasterizerCitro3d::shader_get_default_texture_param(RID p_shader, const StringName& p_name) const {

	return RID();
}

Variant RasterizerCitro3d::shader_get_default_param(RID p_shader, const StringName& p_name) {

	return Variant();
}

/* COMMON MATERIAL API */


RID RasterizerCitro3d::material_create() {
	print("material_create\n");
	return material_owner.make_rid( memnew( Material ) );
}

void RasterizerCitro3d::material_set_shader(RID p_material, RID p_shader) {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->shader=p_shader;

}

RID RasterizerCitro3d::material_get_shader(RID p_material) const {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,RID());
	return material->shader;
}

void RasterizerCitro3d::material_set_param(RID p_material, const StringName& p_param, const Variant& p_value)
{
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);

	Map<StringName,Material::UniformData>::Element *E=material->shader_params.find(p_param);
	if (E)
	{
		if (p_value.get_type()==Variant::NIL) {
			material->shader_params.erase(E);
// 			material->shader_version=0; //get default!
		} else {
			E->get().value=p_value;
			E->get().inuse=true;
		}
	} else {
		if (p_value.get_type()==Variant::NIL)
			return;

		Material::UniformData ud;
		ud.index=-1;
		ud.value=p_value;
		ud.istexture=p_value.get_type()==Variant::_RID; /// cache it being texture
		ud.inuse=true;
		material->shader_params[p_param]=ud; //may be got at some point, or erased
	}
}
Variant RasterizerCitro3d::material_get_param(RID p_material, const StringName& p_param) const
{
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,Variant());

/*
	if (material->shader.is_valid()) {
		//update shader params if necesary
		//make sure the shader is compiled and everything
		//so the actual parameters can be properly retrieved!
		material->shader_cache=shader_owner.get( material->shader );
		if (!material->shader_cache) {
			//invalidate
			material->shader=RID();
			material->shader_cache=NULL;
		} else {

			if (material->shader_cache->dirty_list.in_list())
				_update_shader(material->shader_cache);
			if (material->shader_cache->valid && material->shader_cache->version!=material->shader_version) {
				//validate
				_update_material_shader_params(material);
			}
		}
	}
*/

	if (material->shader_params.has(p_param) && material->shader_params[p_param].inuse)
		return material->shader_params[p_param].value;
	else
		return Variant();
}


void RasterizerCitro3d::material_set_flag(RID p_material, VS::MaterialFlag p_flag,bool p_enabled) {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	ERR_FAIL_INDEX(p_flag,VS::MATERIAL_FLAG_MAX);
	material->flags[p_flag]=p_enabled;

}
bool RasterizerCitro3d::material_get_flag(RID p_material,VS::MaterialFlag p_flag) const {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,false);
	ERR_FAIL_INDEX_V(p_flag,VS::MATERIAL_FLAG_MAX,false);
	return material->flags[p_flag];


}

void RasterizerCitro3d::material_set_depth_draw_mode(RID p_material, VS::MaterialDepthDrawMode p_mode) {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->depth_draw_mode=p_mode;
}

VS::MaterialDepthDrawMode RasterizerCitro3d::material_get_depth_draw_mode(RID p_material) const{

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,VS::MATERIAL_DEPTH_DRAW_ALWAYS);
	return material->depth_draw_mode;
}


void RasterizerCitro3d::material_set_blend_mode(RID p_material,VS::MaterialBlendMode p_mode)
{
	print("material_set_blend_mode\n");
	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->blend_mode=p_mode;
}
VS::MaterialBlendMode RasterizerCitro3d::material_get_blend_mode(RID p_material) const {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,VS::MATERIAL_BLEND_MODE_ADD);
	return material->blend_mode;
}

void RasterizerCitro3d::material_set_line_width(RID p_material,float p_line_width) {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND(!material);
	material->line_width=p_line_width;

}
float RasterizerCitro3d::material_get_line_width(RID p_material) const {

	Material *material = material_owner.get(p_material);
	ERR_FAIL_COND_V(!material,0);

	return material->line_width;
}

/* MESH API */


RID RasterizerCitro3d::mesh_create()
{
	print("mesh_create\n");
	return mesh_owner.make_rid( memnew( Mesh ) );
}

Error RasterizerCitro3d::_surface_set_arrays(Surface *p_surface, uint8_t *p_mem,uint8_t *p_index_mem,const Array& p_arrays,bool p_main) {

	uint32_t stride = p_main ? p_surface->stride : p_surface->local_stride;

	for(int ai=0;ai<VS::ARRAY_MAX;ai++)
	{
		if (ai>=p_arrays.size())
			break;
		if (p_arrays[ai].get_type()==Variant::NIL)
			continue;
		Surface::ArrayData &a=p_surface->array[ai];

		switch(ai) {

			case VS::ARRAY_VERTEX: {

				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::VECTOR3_ARRAY, ERR_INVALID_PARAMETER );

				DVector<Vector3> array = p_arrays[ai];
				ERR_FAIL_COND_V( array.size() != p_surface->array_len, ERR_INVALID_PARAMETER );


				DVector<Vector3>::Read read = array.read();
				const Vector3* src=read.ptr();

				// setting vertices means regenerating the AABB
				AABB aabb;

				float scale=1;

				for (int i=0;i<p_surface->array_len;i++) {
					float vector[3]={ src[i].x, src[i].y, src[i].z };

					memcpy(&p_mem[a.ofs+i*stride], vector, a.size);

					if (i==0) {
						aabb=AABB(src[i],Vector3());
					} else {
						aabb.expand_to( src[i] );
					}
				}

				if (p_main) {
					p_surface->aabb=aabb;
					p_surface->vertex_scale=scale;
				}

			} break;
			case VS::ARRAY_NORMAL: {

				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::VECTOR3_ARRAY, ERR_INVALID_PARAMETER );

				DVector<Vector3> array = p_arrays[ai];
				ERR_FAIL_COND_V( array.size() != p_surface->array_len, ERR_INVALID_PARAMETER );


				DVector<Vector3>::Read read = array.read();
				const Vector3* src=read.ptr();

				// setting vertices means regenerating the AABB
				for (int i=0;i<p_surface->array_len;i++) {
					float vector[3]={ src[i].x, src[i].y, src[i].z };
					memcpy(&p_mem[a.ofs+i*stride], vector, a.size);
				}
			} break;
			case VS::ARRAY_TANGENT: {

				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::REAL_ARRAY, ERR_INVALID_PARAMETER );

				DVector<real_t> array = p_arrays[ai];

				ERR_FAIL_COND_V( array.size() != p_surface->array_len*4, ERR_INVALID_PARAMETER );

				DVector<real_t>::Read read = array.read();
				const real_t* src = read.ptr();

				for (int i=0;i<p_surface->array_len;i++) {

					float xyzw[4]={
						src[i*4+0],
						src[i*4+1],
						src[i*4+2],
						src[i*4+3]
					};

					memcpy(&p_mem[a.ofs+i*stride], xyzw, a.size);

				}

			} break;
			case VS::ARRAY_COLOR: {

				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::COLOR_ARRAY, ERR_INVALID_PARAMETER );

				DVector<Color> array = p_arrays[ai];

				ERR_FAIL_COND_V( array.size() != p_surface->array_len, ERR_INVALID_PARAMETER );

				DVector<Color>::Read read = array.read();
				const Color* src = read.ptr();
				bool alpha=false;

				for (int i=0;i<p_surface->array_len;i++) {

					if (src[i].a<0.98) // tolerate alpha a bit, for crappy exporters
						alpha=true;

					uint8_t colors[4];

					for(int j=0;j<4;j++) {
						colors[j]=CLAMP( int((src[i][j])*255.0), 0,255 );
					}

					memcpy(&p_mem[a.ofs+i*stride], colors, a.size);
				}

				if (p_main)
					p_surface->has_alpha=alpha;

			} break;
			case VS::ARRAY_TEX_UV:
			case VS::ARRAY_TEX_UV2: {

				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::VECTOR3_ARRAY && p_arrays[ai].get_type() != Variant::VECTOR2_ARRAY, ERR_INVALID_PARAMETER );

				DVector<Vector2> array = p_arrays[ai];

				ERR_FAIL_COND_V( array.size() != p_surface->array_len , ERR_INVALID_PARAMETER);

				DVector<Vector2>::Read read = array.read();

				const Vector2 * src=read.ptr();
				float scale=1.0;

				for (int i=0;i<p_surface->array_len;i++) {

					float uv[2]={ src[i].x , src[i].y };

					memcpy(&p_mem[a.ofs+i*stride], uv, a.size);

				}

				if (p_main) {
					if  (ai==VS::ARRAY_TEX_UV) {
						p_surface->uv_scale=scale;
					}
					if  (ai==VS::ARRAY_TEX_UV2) {
						p_surface->uv2_scale=scale;
					}
				}

			} break;
			case VS::ARRAY_WEIGHTS: {
				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::REAL_ARRAY, ERR_INVALID_PARAMETER );

				DVector<real_t> array = p_arrays[ai];

				ERR_FAIL_COND_V( array.size() != p_surface->array_len*VS::ARRAY_WEIGHTS_SIZE, ERR_INVALID_PARAMETER );


				DVector<real_t>::Read read = array.read();

				const real_t * src = read.ptr();

				for (int i=0;i<p_surface->array_len;i++) {

					float data[VS::ARRAY_WEIGHTS_SIZE];
					for (int j=0;j<VS::ARRAY_WEIGHTS_SIZE;j++) {
						data[j]=src[i*VS::ARRAY_WEIGHTS_SIZE+j];
					}

					memcpy(&p_mem[a.ofs+i*stride], data, a.size);
				}

			} break;
			case VS::ARRAY_BONES: {


				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::REAL_ARRAY, ERR_INVALID_PARAMETER );

				DVector<int> array = p_arrays[ai];

				ERR_FAIL_COND_V( array.size() != p_surface->array_len*VS::ARRAY_WEIGHTS_SIZE, ERR_INVALID_PARAMETER );


				DVector<int>::Read read = array.read();

				const int * src = read.ptr();

				p_surface->max_bone=0;

				for (int i=0;i<p_surface->array_len;i++) {

					u8 data[VS::ARRAY_WEIGHTS_SIZE];
					for (int j=0;j<VS::ARRAY_WEIGHTS_SIZE;j++) {
						data[j]=CLAMP(src[i*VS::ARRAY_WEIGHTS_SIZE+j],0,255);
						p_surface->max_bone=MAX(data[j],p_surface->max_bone);

					}

					memcpy(&p_mem[a.ofs+i*stride], data, a.size);
				}

			} break;
			case VS::ARRAY_INDEX: {

// 				ERR_FAIL_COND_V(a.size > 2, ERR_INVALID_DATA);
				ERR_FAIL_COND_V( p_surface->index_array_len<=0, ERR_INVALID_DATA );
				ERR_FAIL_COND_V( p_arrays[ai].get_type() != Variant::INT_ARRAY, ERR_INVALID_PARAMETER );

				DVector<int> indices = p_arrays[ai];
				ERR_FAIL_COND_V( indices.size() == 0, ERR_INVALID_PARAMETER );
				ERR_FAIL_COND_V( indices.size() != p_surface->index_array_len, ERR_INVALID_PARAMETER );

				/* determine wether using 8 or 16 bits indices */

				DVector<int>::Read read = indices.read();
				const int *src=read.ptr();

				for (int i=0;i<p_surface->index_array_len;i++) {
					if (a.size==2) {
						uint16_t v=src[i];
						memcpy(&p_index_mem[i*a.size], &v, a.size);
					} else {
						uint8_t v=src[i];
						memcpy(&p_index_mem[i*a.size], &v, a.size);
					}
				}
			} break;

			default: { ERR_FAIL_V(ERR_INVALID_PARAMETER);}
		}

		p_surface->configured_format|=(1<<ai);
	}

	if (p_surface->format&VS::ARRAY_FORMAT_BONES) {
		//create AABBs for each detected bone
		int total_bones = p_surface->max_bone+1;
		if (p_main) {
			p_surface->skeleton_bone_aabb.resize(total_bones);
			p_surface->skeleton_bone_used.resize(total_bones);
			for(int i=0;i<total_bones;i++)
				p_surface->skeleton_bone_used[i]=false;
		}
		DVector<Vector3> vertices = p_arrays[VS::ARRAY_VERTEX];
		DVector<int> bones = p_arrays[VS::ARRAY_BONES];
		DVector<float> weights = p_arrays[VS::ARRAY_WEIGHTS];

		bool any_valid=false;

		if (vertices.size() && bones.size()==vertices.size()*4 && weights.size()==bones.size()) {
			//print_line("MAKING SKELETHONG");
			int vs = vertices.size();
			DVector<Vector3>::Read rv =vertices.read();
			DVector<int>::Read rb=bones.read();
			DVector<float>::Read rw=weights.read();

			Vector<bool> first;
			first.resize(total_bones);
			for(int i=0;i<total_bones;i++) {
				first[i]=p_main;
			}
			AABB *bptr = p_surface->skeleton_bone_aabb.ptr();
			bool *fptr=first.ptr();
			bool *usedptr=p_surface->skeleton_bone_used.ptr();

			for(int i=0;i<vs;i++) {

				Vector3 v = rv[i];
				for(int j=0;j<4;j++) {

					int idx = rb[i*4+j];
					float w = rw[i*4+j];
					if (w==0)
						continue;//break;
					ERR_FAIL_INDEX_V(idx,total_bones,ERR_INVALID_DATA);

					if (fptr[idx]) {
						bptr[idx].pos=v;
						fptr[idx]=false;
						any_valid=true;
					} else {
						bptr[idx].expand_to(v);
					}
					usedptr[idx]=true;
				}
			}
		}

		if (p_main && !any_valid) {

			p_surface->skeleton_bone_aabb.clear();
			p_surface->skeleton_bone_used.clear();
		}
	}

	return OK;
}

void RasterizerCitro3d::mesh_add_surface(RID p_mesh,VS::PrimitiveType p_primitive,const Array& p_arrays,const Array& p_blend_shapes,bool p_alpha_sort)
{
	print("mesh_add_surface\n");
	ERR_FAIL_COND(p_primitive < VS::PRIMITIVE_TRIANGLES);
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);

	ERR_FAIL_INDEX( p_primitive, VS::PRIMITIVE_MAX );
	ERR_FAIL_COND(p_arrays.size()!=VS::ARRAY_MAX);

	uint32_t format=0;

	// validation
	int index_array_len=0;
	int array_len=0;

	for(int i=0;i<p_arrays.size();i++) {

		if (p_arrays[i].get_type()==Variant::NIL)
			continue;

		format|=(1<<i);

		if (i==VS::ARRAY_VERTEX) {

			array_len=Vector3Array(p_arrays[i]).size();
			ERR_FAIL_COND(array_len==0);
		} else if (i==VS::ARRAY_INDEX) {

			index_array_len=IntArray(p_arrays[i]).size();
		}
	}

	ERR_FAIL_COND((format&VS::ARRAY_FORMAT_VERTEX)==0); // mandatory

	ERR_FAIL_COND( mesh->morph_target_count!=p_blend_shapes.size() );
	if (mesh->morph_target_count) {
		//validate format for morphs
		for(int i=0;i<p_blend_shapes.size();i++) {

			uint32_t bsformat=0;
			Array arr = p_blend_shapes[i];
			for(int j=0;j<arr.size();j++) {


				if (arr[j].get_type()!=Variant::NIL)
					bsformat|=(1<<j);
			}

			ERR_FAIL_COND( (bsformat)!=(format&(VS::ARRAY_FORMAT_BONES-1)));
		}
	}

	Surface *surface = memnew( Surface );
	ERR_FAIL_COND( !surface );

	int total_elem_size=0;

	for (int i=0;i<VS::ARRAY_MAX;i++)
	{
		Surface::ArrayData&ad=surface->array[i];
		ad.size=0;
		ad.ofs=0;
		int elem_size=0;
		int elem_count=0;
		bool valid_local=true;
		bool normalize=false;
		bool bind=false;

		if (!(format&(1<<i))) // no array
			continue;

		switch(i) {

			case VS::ARRAY_VERTEX: {
				print("ARRAY_VERTEX\n");
				elem_size=3*sizeof(float);
				bind=true;
				elem_count=3;
			} break;
			case VS::ARRAY_NORMAL: {
				print("ARRAY_NORMAL\n");
				elem_size=3*sizeof(float);
				bind=true;
				elem_count=3;
			} break;
			case VS::ARRAY_TANGENT: {
				print("ARRAY_TANGENT\n");
				continue;
				elem_size=4*sizeof(float);
				bind=true;
				elem_count=4;
			} break;
			case VS::ARRAY_COLOR: {
				print("ARRAY_COLOR\n");
				continue;
				elem_size=4*sizeof(uint8_t); /* RGBA */
				elem_count=4;
				bind=true;
				normalize=true;
			} break;
			case VS::ARRAY_TEX_UV2:
				print("ARRAY_TEX_UV2\n");
				continue;
			case VS::ARRAY_TEX_UV: {
				print("ARRAY_TEX_UV\n");
				elem_size=2*sizeof(float); // vertex
				bind=true;
				elem_count=2;
			} break;
			case VS::ARRAY_WEIGHTS: {
				print("ARRAY_WEIGHTS\n");
				continue;
				elem_size=VS::ARRAY_WEIGHTS_SIZE*sizeof(float);
				valid_local=false;
				bind=true;
				elem_count=4;
			} break;
			case VS::ARRAY_BONES: {
				print("ARRAY_BONES\n");
				continue;
				elem_size=VS::ARRAY_WEIGHTS_SIZE*sizeof(u8);
				valid_local=false;
				bind=true;
				elem_count=4;
			} break;
			case VS::ARRAY_INDEX: {
				print("ARRAY_INDEX\n");
				if (index_array_len<=0) {
					ERR_PRINT("index_array_len==NO_INDEX_ARRAY");
					break;
				}
				ERR_FAIL_COND(array_len > (1<<16)); // 32 bit index not supported
				
				/* determine wether using 8 or 16 bits indices */
				if (array_len > (1<<8)) {
					elem_size=2; // C3D_UNSIGNED_SHORT
				} else {
					elem_size=1; // C3D_UNSIGNED_BYTE
				}

				surface->index_array_len=index_array_len; // only way it can exist
				ad.ofs=0;
				ad.size=elem_size;
				
				continue;
			} break;
			default: {
				ERR_FAIL( );
			}
		}

		ad.ofs=total_elem_size;
		ad.size=elem_size;
// 		ad.datatype=datatype;
		ad.normalize=normalize;
		ad.bind=bind;
		ad.count=elem_count;
		total_elem_size+=elem_size;
		if (valid_local) {
			surface->local_stride+=elem_size;
			surface->morph_format|=(1<<i);
		}
	}

	surface->stride=total_elem_size;
	surface->array_len=array_len;
	surface->format=format;
	surface->primitive=p_primitive;
	surface->morph_target_count=mesh->morph_target_count;
	surface->configured_format=0;
	surface->mesh=mesh;
// 	if (keep_copies) {
// 		surface->data=p_arrays;
// 		surface->morph_data=p_blend_shapes;
// 	}

	surface->array_local = (u8*)linearAlloc(surface->array_len*surface->stride);
// 	u8 *array_ptr= (u8*)linearAlloc(surface->array_len*surface->stride);
// 	u8 *index_array_ptr=NULL;
	
// 	DVector<uint8_t> array_pre_vbo;
// 	DVector<uint8_t>::Write vaw;
// 	DVector<uint8_t> index_array_pre_vbo;
// 	DVector<uint8_t>::Write iaw;

	/* create pointers */
// 	array_pre_vbo.resize(surface->array_len*surface->stride);
// 	vaw = array_pre_vbo.write();
// 	array_ptr=vaw.ptr();

	if (surface->index_array_len)
	{
// 		index_array_pre_vbo.resize(surface->index_array_len*surface->array[VS::ARRAY_INDEX].size);
// 		iaw = index_array_pre_vbo.write();
// 		index_array_ptr=iaw.ptr();
		surface->index_array_local = (u8*) linearAlloc(surface->index_array_len*surface->array[VS::ARRAY_INDEX].size);
// 		index_array_ptr = (u8*) linearAlloc(surface->index_array_len*surface->array[VS::ARRAY_INDEX].size);
	}

	_surface_set_arrays(surface,surface->array_local,surface->index_array_local,p_arrays,true);

	/* create buffers!! */
	
	
// 	glGenBuffers(1,&surface->vertex_id);
// 	ERR_FAIL_COND(surface->vertex_id==0);
// 	glBindBuffer(GL_ARRAY_BUFFER,surface->vertex_id);
// 	glBufferData(GL_ARRAY_BUFFER,surface->array_len*surface->stride,array_ptr,GL_STATIC_DRAW);
// 	glBindBuffer(GL_ARRAY_BUFFER,0); //unbind
// 	if (surface->index_array_len) {
// 
// 		glGenBuffers(1,&surface->index_id);
// 		ERR_FAIL_COND(surface->index_id==0);
// 		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,surface->index_id);
// 		glBufferData(GL_ELEMENT_ARRAY_BUFFER,index_array_len*surface->array[VS::ARRAY_INDEX].size,index_array_ptr,GL_STATIC_DRAW);
// 		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0); //unbind
// 
// 	}

	mesh->surfaces.push_back(surface);
}

void RasterizerCitro3d::mesh_add_custom_surface(RID p_mesh,const Variant& p_dat)
{
	ERR_EXPLAIN("Rasterizer does not support custom surfaces. Running on wrong platform?");
	ERR_FAIL_V();
}

Array RasterizerCitro3d::mesh_get_surface_arrays(RID p_mesh,int p_surface) const
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,Array());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Array() );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, Array() );

	return surface->data;
}
Array RasterizerCitro3d::mesh_get_surface_morph_arrays(RID p_mesh,int p_surface) const
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,Array());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), Array() );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, Array() );

	return surface->morph_data;
}


void RasterizerCitro3d::mesh_set_morph_target_count(RID p_mesh,int p_amount)
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_COND( mesh->surfaces.size()!=0 );

	mesh->morph_target_count=p_amount;
}

int RasterizerCitro3d::mesh_get_morph_target_count(RID p_mesh) const
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,-1);

	return mesh->morph_target_count;
}

void RasterizerCitro3d::mesh_set_morph_target_mode(RID p_mesh,VS::MorphTargetMode p_mode)
{
	ERR_FAIL_INDEX(p_mode,2);
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);

	mesh->morph_target_mode=p_mode;
}

VS::MorphTargetMode RasterizerCitro3d::mesh_get_morph_target_mode(RID p_mesh) const
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,VS::MORPH_MODE_NORMALIZED);

	return mesh->morph_target_mode;
}



void RasterizerCitro3d::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material,bool p_owned)
{
	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_surface, mesh->surfaces.size() );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND( !surface);

	if (surface->material_owned && surface->material.is_valid())
		free(surface->material);

	surface->material_owned=p_owned;
	surface->material=p_material;
}

RID RasterizerCitro3d::mesh_surface_get_material(RID p_mesh, int p_surface) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,RID());
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), RID() );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, RID() );

	return surface->material;
}

int RasterizerCitro3d::mesh_surface_get_array_len(RID p_mesh, int p_surface) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,-1);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), -1 );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, -1 );

	Vector3Array arr = surface->data[VS::ARRAY_VERTEX];
	return arr.size();

}

int RasterizerCitro3d::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,-1);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), -1 );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, -1 );

	IntArray arr = surface->data[VS::ARRAY_INDEX];
	return arr.size();

}
uint32_t RasterizerCitro3d::mesh_surface_get_format(RID p_mesh, int p_surface) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,0);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0 );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, 0 );

	return surface->format;
}
VS::PrimitiveType RasterizerCitro3d::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,VS::PRIMITIVE_POINTS);
	ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), VS::PRIMITIVE_POINTS );
	Surface *surface = mesh->surfaces[p_surface];
	ERR_FAIL_COND_V( !surface, VS::PRIMITIVE_POINTS );

	return surface->primitive;
}

void RasterizerCitro3d::mesh_remove_surface(RID p_mesh,int p_index) {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);
	ERR_FAIL_INDEX(p_index, mesh->surfaces.size() );
	Surface *surface = mesh->surfaces[p_index];
	ERR_FAIL_COND( !surface);

	memdelete( mesh->surfaces[p_index] );
	mesh->surfaces.remove(p_index);

}
int RasterizerCitro3d::mesh_get_surface_count(RID p_mesh) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,-1);

	return mesh->surfaces.size();
}

AABB RasterizerCitro3d::mesh_get_aabb(RID p_mesh,RID p_skeleton) const {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,AABB());

	AABB aabb;

	for (int i=0;i<mesh->surfaces.size();i++) {

		if (i==0)
			aabb=mesh->surfaces[i]->aabb;
		else
			aabb.merge_with(mesh->surfaces[i]->aabb);
	}

	return aabb;
}

void RasterizerCitro3d::mesh_set_custom_aabb(RID p_mesh,const AABB& p_aabb) {

	Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND(!mesh);

	mesh->custom_aabb=p_aabb;
}

AABB RasterizerCitro3d::mesh_get_custom_aabb(RID p_mesh) const {

	const Mesh *mesh = mesh_owner.get( p_mesh );
	ERR_FAIL_COND_V(!mesh,AABB());

	return mesh->custom_aabb;
}

/* MULTIMESH API */

RID RasterizerCitro3d::multimesh_create()
{
	print("multimesh_create\n");
	return multimesh_owner.make_rid( memnew( MultiMesh ));
}

void RasterizerCitro3d::multimesh_set_instance_count(RID p_multimesh,int p_count) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->elements.clear(); // make sure to delete everything, so it "fails" in all implementations
	multimesh->elements.resize(p_count);
}
int RasterizerCitro3d::multimesh_get_instance_count(RID p_multimesh) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,-1);

	return multimesh->elements.size();
}

void RasterizerCitro3d::multimesh_set_mesh(RID p_multimesh,RID p_mesh) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);

	multimesh->mesh=p_mesh;

}
void RasterizerCitro3d::multimesh_set_aabb(RID p_multimesh,const AABB& p_aabb) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	multimesh->aabb=p_aabb;
}
void RasterizerCitro3d::multimesh_instance_set_transform(RID p_multimesh,int p_index,const Transform& p_transform) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	ERR_FAIL_INDEX(p_index,multimesh->elements.size());
	multimesh->elements[p_index].xform=p_transform;

}
void RasterizerCitro3d::multimesh_instance_set_color(RID p_multimesh,int p_index,const Color& p_color) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh)
	ERR_FAIL_INDEX(p_index,multimesh->elements.size());
	multimesh->elements[p_index].color=p_color;

}

RID RasterizerCitro3d::multimesh_get_mesh(RID p_multimesh) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,RID());

	return multimesh->mesh;
}
AABB RasterizerCitro3d::multimesh_get_aabb(RID p_multimesh) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,AABB());

	return multimesh->aabb;
}

Transform RasterizerCitro3d::multimesh_instance_get_transform(RID p_multimesh,int p_index) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,Transform());

	ERR_FAIL_INDEX_V(p_index,multimesh->elements.size(),Transform());

	return multimesh->elements[p_index].xform;

}
Color RasterizerCitro3d::multimesh_instance_get_color(RID p_multimesh,int p_index) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,Color());
	ERR_FAIL_INDEX_V(p_index,multimesh->elements.size(),Color());

	return multimesh->elements[p_index].color;
}

void RasterizerCitro3d::multimesh_set_visible_instances(RID p_multimesh,int p_visible) {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND(!multimesh);
	multimesh->visible=p_visible;

}

int RasterizerCitro3d::multimesh_get_visible_instances(RID p_multimesh) const {

	MultiMesh *multimesh = multimesh_owner.get(p_multimesh);
	ERR_FAIL_COND_V(!multimesh,-1);
	return multimesh->visible;

}

/* IMMEDIATE API */


RID RasterizerCitro3d::immediate_create() {

	print("immediate_create\n");
	Immediate *im = memnew( Immediate );
	return immediate_owner.make_rid(im);

}

void RasterizerCitro3d::immediate_begin(RID p_immediate,VS::PrimitiveType p_rimitive,RID p_texture)
{
	print("immediate_begin\n");
}
void RasterizerCitro3d::immediate_vertex(RID p_immediate,const Vector3& p_vertex){


}
void RasterizerCitro3d::immediate_normal(RID p_immediate,const Vector3& p_normal){


}
void RasterizerCitro3d::immediate_tangent(RID p_immediate,const Plane& p_tangent){


}
void RasterizerCitro3d::immediate_color(RID p_immediate,const Color& p_color){


}
void RasterizerCitro3d::immediate_uv(RID p_immediate,const Vector2& tex_uv){


}
void RasterizerCitro3d::immediate_uv2(RID p_immediate,const Vector2& tex_uv){


}

void RasterizerCitro3d::immediate_end(RID p_immediate){

	print("immediate_end\n");
}
void RasterizerCitro3d::immediate_clear(RID p_immediate) {


}

AABB RasterizerCitro3d::immediate_get_aabb(RID p_immediate) const {

	return AABB(Vector3(-1,-1,-1),Vector3(2,2,2));
}

void RasterizerCitro3d::immediate_set_material(RID p_immediate,RID p_material) {

	Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND(!im);
	im->material=p_material;

}

RID RasterizerCitro3d::immediate_get_material(RID p_immediate) const {

	const Immediate *im = immediate_owner.get(p_immediate);
	ERR_FAIL_COND_V(!im,RID());
	return im->material;

}

/* PARTICLES API */

RID RasterizerCitro3d::particles_create()
{
	Particles *particles = memnew( Particles );
	ERR_FAIL_COND_V(!particles,RID());
	return particles_owner.make_rid(particles);
}

void RasterizerCitro3d::particles_set_amount(RID p_particles, int p_amount)
{
	ERR_FAIL_COND(p_amount<1);
	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.amount=p_amount;
}

int RasterizerCitro3d::particles_get_amount(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.amount;

}

void RasterizerCitro3d::particles_set_emitting(RID p_particles, bool p_emitting) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.emitting=p_emitting;;

}
bool RasterizerCitro3d::particles_is_emitting(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,false);
	return particles->data.emitting;

}

void RasterizerCitro3d::particles_set_visibility_aabb(RID p_particles, const AABB& p_visibility) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.visibility_aabb=p_visibility;

}

void RasterizerCitro3d::particles_set_emission_half_extents(RID p_particles, const Vector3& p_half_extents) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);

	particles->data.emission_half_extents=p_half_extents;
}
Vector3 RasterizerCitro3d::particles_get_emission_half_extents(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,Vector3());

	return particles->data.emission_half_extents;
}

void RasterizerCitro3d::particles_set_emission_base_velocity(RID p_particles, const Vector3& p_base_velocity) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);

	particles->data.emission_base_velocity=p_base_velocity;
}

Vector3 RasterizerCitro3d::particles_get_emission_base_velocity(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,Vector3());

	return particles->data.emission_base_velocity;
}


void RasterizerCitro3d::particles_set_emission_points(RID p_particles, const DVector<Vector3>& p_points) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);

	particles->data.emission_points=p_points;
}

DVector<Vector3> RasterizerCitro3d::particles_get_emission_points(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,DVector<Vector3>());

	return particles->data.emission_points;

}

void RasterizerCitro3d::particles_set_gravity_normal(RID p_particles, const Vector3& p_normal) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);

	particles->data.gravity_normal=p_normal;

}
Vector3 RasterizerCitro3d::particles_get_gravity_normal(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,Vector3());

	return particles->data.gravity_normal;
}


AABB RasterizerCitro3d::particles_get_visibility_aabb(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,AABB());
	return particles->data.visibility_aabb;

}

void RasterizerCitro3d::particles_set_variable(RID p_particles, VS::ParticleVariable p_variable,float p_value) {

	ERR_FAIL_INDEX(p_variable,VS::PARTICLE_VAR_MAX);

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.particle_vars[p_variable]=p_value;

}
float RasterizerCitro3d::particles_get_variable(RID p_particles, VS::ParticleVariable p_variable) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.particle_vars[p_variable];
}

void RasterizerCitro3d::particles_set_randomness(RID p_particles, VS::ParticleVariable p_variable,float p_randomness) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.particle_randomness[p_variable]=p_randomness;

}
float RasterizerCitro3d::particles_get_randomness(RID p_particles, VS::ParticleVariable p_variable) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.particle_randomness[p_variable];

}

void RasterizerCitro3d::particles_set_color_phases(RID p_particles, int p_phases) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	ERR_FAIL_COND( p_phases<0 || p_phases>VS::MAX_PARTICLE_COLOR_PHASES );
	particles->data.color_phase_count=p_phases;

}
int RasterizerCitro3d::particles_get_color_phases(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.color_phase_count;
}


void RasterizerCitro3d::particles_set_color_phase_pos(RID p_particles, int p_phase, float p_pos) {

	ERR_FAIL_INDEX(p_phase, VS::MAX_PARTICLE_COLOR_PHASES);
	if (p_pos<0.0)
		p_pos=0.0;
	if (p_pos>1.0)
		p_pos=1.0;

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.color_phases[p_phase].pos=p_pos;

}
float RasterizerCitro3d::particles_get_color_phase_pos(RID p_particles, int p_phase) const {

	ERR_FAIL_INDEX_V(p_phase, VS::MAX_PARTICLE_COLOR_PHASES, -1.0);

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.color_phases[p_phase].pos;

}

void RasterizerCitro3d::particles_set_color_phase_color(RID p_particles, int p_phase, const Color& p_color) {

	ERR_FAIL_INDEX(p_phase, VS::MAX_PARTICLE_COLOR_PHASES);
	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.color_phases[p_phase].color=p_color;

	//update alpha
	particles->has_alpha=false;
	for(int i=0;i<VS::MAX_PARTICLE_COLOR_PHASES;i++) {
		if (particles->data.color_phases[i].color.a<0.99)
			particles->has_alpha=true;
	}

}

Color RasterizerCitro3d::particles_get_color_phase_color(RID p_particles, int p_phase) const {

	ERR_FAIL_INDEX_V(p_phase, VS::MAX_PARTICLE_COLOR_PHASES, Color());

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,Color());
	return particles->data.color_phases[p_phase].color;

}

void RasterizerCitro3d::particles_set_attractors(RID p_particles, int p_attractors) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	ERR_FAIL_COND( p_attractors<0 || p_attractors>VisualServer::MAX_PARTICLE_ATTRACTORS );
	particles->data.attractor_count=p_attractors;

}
int RasterizerCitro3d::particles_get_attractors(RID p_particles) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,-1);
	return particles->data.attractor_count;
}

void RasterizerCitro3d::particles_set_attractor_pos(RID p_particles, int p_attractor, const Vector3& p_pos) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	ERR_FAIL_INDEX(p_attractor,particles->data.attractor_count);
	particles->data.attractors[p_attractor].pos=p_pos;;
}
Vector3 RasterizerCitro3d::particles_get_attractor_pos(RID p_particles,int p_attractor) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,Vector3());
	ERR_FAIL_INDEX_V(p_attractor,particles->data.attractor_count,Vector3());
	return particles->data.attractors[p_attractor].pos;
}

void RasterizerCitro3d::particles_set_attractor_strength(RID p_particles, int p_attractor, float p_force) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	ERR_FAIL_INDEX(p_attractor,particles->data.attractor_count);
	particles->data.attractors[p_attractor].force=p_force;
}

float RasterizerCitro3d::particles_get_attractor_strength(RID p_particles,int p_attractor) const {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,0);
	ERR_FAIL_INDEX_V(p_attractor,particles->data.attractor_count,0);
	return particles->data.attractors[p_attractor].force;
}

void RasterizerCitro3d::particles_set_material(RID p_particles, RID p_material,bool p_owned) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	if (particles->material_owned && particles->material.is_valid())
		free(particles->material);

	particles->material_owned=p_owned;

	particles->material=p_material;

}
RID RasterizerCitro3d::particles_get_material(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,RID());
	return particles->material;

}

void RasterizerCitro3d::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.local_coordinates=p_enable;

}

bool RasterizerCitro3d::particles_is_using_local_coordinates(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,false);
	return particles->data.local_coordinates;
}
bool RasterizerCitro3d::particles_has_height_from_velocity(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,false);
	return particles->data.height_from_velocity;
}

void RasterizerCitro3d::particles_set_height_from_velocity(RID p_particles, bool p_enable) {

	Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND(!particles);
	particles->data.height_from_velocity=p_enable;

}

AABB RasterizerCitro3d::particles_get_aabb(RID p_particles) const {

	const Particles* particles = particles_owner.get( p_particles );
	ERR_FAIL_COND_V(!particles,AABB());
	return particles->data.visibility_aabb;
}

/* SKELETON API */

RID RasterizerCitro3d::skeleton_create() {

	Skeleton *skeleton = memnew( Skeleton );
	ERR_FAIL_COND_V(!skeleton,RID());
	return skeleton_owner.make_rid( skeleton );
}
void RasterizerCitro3d::skeleton_resize(RID p_skeleton,int p_bones) {

	Skeleton *skeleton = skeleton_owner.get( p_skeleton );
	ERR_FAIL_COND(!skeleton);
	if (p_bones == skeleton->bones.size()) {
		return;
	};

	skeleton->bones.resize(p_bones);

}
int RasterizerCitro3d::skeleton_get_bone_count(RID p_skeleton) const {

	Skeleton *skeleton = skeleton_owner.get( p_skeleton );
	ERR_FAIL_COND_V(!skeleton, -1);
	return skeleton->bones.size();
}
void RasterizerCitro3d::skeleton_bone_set_transform(RID p_skeleton,int p_bone, const Transform& p_transform) {

	Skeleton *skeleton = skeleton_owner.get( p_skeleton );
	ERR_FAIL_COND(!skeleton);
	ERR_FAIL_INDEX( p_bone, skeleton->bones.size() );

	skeleton->bones[p_bone] = p_transform;
}

Transform RasterizerCitro3d::skeleton_bone_get_transform(RID p_skeleton,int p_bone) {

	Skeleton *skeleton = skeleton_owner.get( p_skeleton );
	ERR_FAIL_COND_V(!skeleton, Transform());
	ERR_FAIL_INDEX_V( p_bone, skeleton->bones.size(), Transform() );

	// something
	return skeleton->bones[p_bone];
}


/* LIGHT API */

RID RasterizerCitro3d::light_create(VS::LightType p_type) {
	print("light_create\n");
	Light *light = memnew( Light );
	light->type=p_type;
	return light_owner.make_rid(light);
}

VS::LightType RasterizerCitro3d::light_get_type(RID p_light) const {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light,VS::LIGHT_OMNI);
	return light->type;
}

void RasterizerCitro3d::light_set_color(RID p_light,VS::LightColor p_type, const Color& p_color) {
	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	ERR_FAIL_INDEX( p_type, 3 );
	light->colors[p_type]=p_color;
}
Color RasterizerCitro3d::light_get_color(RID p_light,VS::LightColor p_type) const {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light, Color());
	ERR_FAIL_INDEX_V( p_type, 3, Color() );
	return light->colors[p_type];
}

void RasterizerCitro3d::light_set_shadow(RID p_light,bool p_enabled) {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->shadow_enabled=p_enabled;
}

bool RasterizerCitro3d::light_has_shadow(RID p_light) const {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light,false);
	return light->shadow_enabled;
}

void RasterizerCitro3d::light_set_volumetric(RID p_light,bool p_enabled) {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->volumetric_enabled=p_enabled;

}
bool RasterizerCitro3d::light_is_volumetric(RID p_light) const {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light,false);
	return light->volumetric_enabled;
}

void RasterizerCitro3d::light_set_projector(RID p_light,RID p_texture) {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND(!light);
	light->projector=p_texture;
}
RID RasterizerCitro3d::light_get_projector(RID p_light) const {

	Light *light = light_owner.get(p_light);
	ERR_FAIL_COND_V(!light,RID());
	return light->projector;
}

void RasterizerCitro3d::light_set_var(RID p_light, VS::LightParam p_var, float p_value) {

	Light * light = light_owner.get( p_light );
	ERR_FAIL_COND(!light);
	ERR_FAIL_INDEX( p_var, VS::LIGHT_PARAM_MAX );

	light->vars[p_var]=p_value;
}
float RasterizerCitro3d::light_get_var(RID p_light, VS::LightParam p_var) const {

	Light * light = light_owner.get( p_light );
	ERR_FAIL_COND_V(!light,0);

	ERR_FAIL_INDEX_V( p_var, VS::LIGHT_PARAM_MAX,0 );

	return light->vars[p_var];
}

void RasterizerCitro3d::light_set_operator(RID p_light,VS::LightOp p_op) {

	Light * light = light_owner.get( p_light );
	ERR_FAIL_COND(!light);


};

VS::LightOp RasterizerCitro3d::light_get_operator(RID p_light) const {

	return VS::LightOp(0);
};

void RasterizerCitro3d::light_omni_set_shadow_mode(RID p_light,VS::LightOmniShadowMode p_mode) {


}

VS::LightOmniShadowMode RasterizerCitro3d::light_omni_get_shadow_mode(RID p_light) const{

	return VS::LightOmniShadowMode(0);
}

void RasterizerCitro3d::light_directional_set_shadow_mode(RID p_light,VS::LightDirectionalShadowMode p_mode) {


}

VS::LightDirectionalShadowMode RasterizerCitro3d::light_directional_get_shadow_mode(RID p_light) const {

	return VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
}

void RasterizerCitro3d::light_directional_set_shadow_param(RID p_light,VS::LightDirectionalShadowParam p_param, float p_value) {


}

float RasterizerCitro3d::light_directional_get_shadow_param(RID p_light,VS::LightDirectionalShadowParam p_param) const {

	return 0;
}


AABB RasterizerCitro3d::light_get_aabb(RID p_light) const {

	Light *light = light_owner.get( p_light );
	ERR_FAIL_COND_V(!light,AABB());

	switch( light->type ) {

		case VS::LIGHT_SPOT: {

			float len=light->vars[VS::LIGHT_PARAM_RADIUS];
			float size=Math::tan(Math::deg2rad(light->vars[VS::LIGHT_PARAM_SPOT_ANGLE]))*len;
			return AABB( Vector3( -size,-size,-len ), Vector3( size*2, size*2, len ) );
		} break;
		case VS::LIGHT_OMNI: {

			float r = light->vars[VS::LIGHT_PARAM_RADIUS];
			return AABB( -Vector3(r,r,r), Vector3(r,r,r)*2 );
		} break;
		case VS::LIGHT_DIRECTIONAL: {

			return AABB();
		} break;
		default: {}
	}

	ERR_FAIL_V( AABB() );
}


RID RasterizerCitro3d::light_instance_create(RID p_light) {

	Light *light = light_owner.get( p_light );
	ERR_FAIL_COND_V(!light, RID());

	LightInstance *light_instance = memnew( LightInstance );

	light_instance->light=p_light;
	light_instance->base=light;

	return light_instance_owner.make_rid( light_instance );
}
void RasterizerCitro3d::light_instance_set_transform(RID p_light_instance,const Transform& p_transform) {

	LightInstance *lighti = light_instance_owner.get( p_light_instance );
	ERR_FAIL_COND(!lighti);
	lighti->transform=p_transform;
}

bool RasterizerCitro3d::light_instance_has_shadow(RID p_light_instance) const {

	return false;

}


bool RasterizerCitro3d::light_instance_assign_shadow(RID p_light_instance) {

	return false;

}


Rasterizer::ShadowType RasterizerCitro3d::light_instance_get_shadow_type(RID p_light_instance) const {

	LightInstance *lighti = light_instance_owner.get( p_light_instance );
	ERR_FAIL_COND_V(!lighti,Rasterizer::SHADOW_NONE);

	switch(lighti->base->type) {

		case VS::LIGHT_DIRECTIONAL: return SHADOW_PSM; break;
		case VS::LIGHT_OMNI: return SHADOW_DUAL_PARABOLOID; break;
		case VS::LIGHT_SPOT: return SHADOW_SIMPLE; break;
	}

	return Rasterizer::SHADOW_NONE;
}

Rasterizer::ShadowType RasterizerCitro3d::light_instance_get_shadow_type(RID p_light_instance,bool p_far) const {

	return SHADOW_NONE;
}
void RasterizerCitro3d::light_instance_set_shadow_transform(RID p_light_instance, int p_index, const CameraMatrix& p_camera, const Transform& p_transform, float p_split_near,float p_split_far) {


}

int RasterizerCitro3d::light_instance_get_shadow_passes(RID p_light_instance) const {

	return 0;
}

bool RasterizerCitro3d::light_instance_get_pssm_shadow_overlap(RID p_light_instance) const {

	return false;
}


void RasterizerCitro3d::light_instance_set_custom_transform(RID p_light_instance, int p_index, const CameraMatrix& p_camera, const Transform& p_transform, float p_split_near,float p_split_far) {

	LightInstance *lighti = light_instance_owner.get( p_light_instance );
	ERR_FAIL_COND(!lighti);

	ERR_FAIL_COND(lighti->base->type!=VS::LIGHT_DIRECTIONAL);
	ERR_FAIL_INDEX(p_index,1);

	lighti->custom_projection=p_camera;
	lighti->custom_transform=p_transform;

}
void RasterizerCitro3d::shadow_clear_near() {


}

bool RasterizerCitro3d::shadow_allocate_near(RID p_light) {

	return false;
}

bool RasterizerCitro3d::shadow_allocate_far(RID p_light) {

	return false;
}

/* PARTICLES INSTANCE */

RID RasterizerCitro3d::particles_instance_create(RID p_particles) {

	ERR_FAIL_COND_V(!particles_owner.owns(p_particles),RID());
	ParticlesInstance *particles_instance = memnew( ParticlesInstance );
	ERR_FAIL_COND_V(!particles_instance, RID() );
	particles_instance->particles=p_particles;
	return particles_instance_owner.make_rid(particles_instance);
}

void RasterizerCitro3d::particles_instance_set_transform(RID p_particles_instance,const Transform& p_transform) {

	ParticlesInstance *particles_instance=particles_instance_owner.get(p_particles_instance);
	ERR_FAIL_COND(!particles_instance);
	particles_instance->transform=p_transform;
}


/* RENDER API */
/* all calls (inside begin/end shadow) are always warranted to be in the following order: */


RID RasterizerCitro3d::viewport_data_create()
{
	print("viewport_data_create\n");
	return RID();
}

RID RasterizerCitro3d::render_target_create()
{
	print("render_target_create\n");
	RenderTarget* rt = memnew(RenderTarget);	
	Texture *texture = memnew(Texture);
	
	rt->target = C3D_RenderTargetCreate(480, 800, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(rt->target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetClear(rt->target, C3D_CLEAR_ALL, 0xFFFFFFFF, 0);
	
	rt->texture_ptr = texture;
	rt->texture = texture_owner.make_rid( texture );
	
	return render_target_owner.make_rid( rt );
}
void RasterizerCitro3d::render_target_set_size(RID p_render_target, int p_width, int p_height)
{
	RenderTarget* rt = render_target_owner.get( p_render_target );
	
}
RID RasterizerCitro3d::render_target_get_texture(RID p_render_target) const
{
	print("render_target_get_texture\n");
	const RenderTarget *rt = render_target_owner.get(p_render_target);
	ERR_FAIL_COND_V(!rt,RID());
	return rt->texture;
}
bool RasterizerCitro3d::render_target_renedered_in_frame(RID p_render_target)
{
	RenderTarget *rt = render_target_owner.get(p_render_target);
	ERR_FAIL_COND_V(!rt,false);
// 	return rt->last_pass==frame;
	return false;
}


void RasterizerCitro3d::begin_frame()
{
	print("begin_frame\n");
	draw_next_frame = false;
	
	float time = (OS::get_singleton()->get_ticks_usec()/1000); // get msec
	time/=1000.0; // make secs
	time_delta=time-last_time;
	last_time=time;
}

void RasterizerCitro3d::capture_viewport(Image* r_capture) {


}


void RasterizerCitro3d::clear_viewport(const Color& p_color)
{
	print("clear_viewport\n");
	RenderTarget* rt = current_rt ? current_rt : base_framebuffer;
	
	u32 c = (u8)(p_color.r*255);
	c <<= 8;
	c |= (u8)(p_color.g*255);
	c <<= 8;
	c |= (u8)(p_color.b*255);
	c <<= 8;
	c |= (u8)(p_color.a*255);
	
	rt->target->renderBuf.clearColor = c;
	C3D_RenderBufClear(&rt->target->renderBuf);
};

void RasterizerCitro3d::set_viewport(const VS::ViewportRect& p_viewport)
{
	print("set_viewport %d %d %d %d\n", p_viewport.x, p_viewport.y, p_viewport.width, p_viewport.height);
	u32 top = p_viewport.height - p_viewport.y;
// 	C3D_SetViewport(top, p_viewport.x, p_viewport.height, p_viewport.width);
	C3D_SetViewport(0, 0, p_viewport.height, p_viewport.width);
}

void RasterizerCitro3d::set_render_target(RID p_render_target, bool p_transparent_bg, bool p_vflip)
{
	print("set_render_target\n");
	current_rt_vflip=p_vflip;
	
	if (p_render_target.is_valid())
	{
		RenderTarget *rt = render_target_owner.get(p_render_target);
		ERR_FAIL_COND(!rt);
		ERR_FAIL_COND(!rt->target);
		C3D_RenderBufBind(&rt->target->renderBuf);
		current_rt=rt;
		current_rt_transparent=p_transparent_bg;
	}
	else
	{
		print("null target\n");
		current_rt = NULL;
		current_rt_transparent = false;
		C3D_RenderBufBind(&base_framebuffer->target->renderBuf);
	}
}


void RasterizerCitro3d::begin_scene(RID p_viewport_data,RID p_env,VS::ScenarioDebugMode p_debug)
{
	print("begin_scene\n");
	opaque_render_list.clear();
	alpha_render_list.clear();
	
	RenderTarget* rt = current_rt ? current_rt : base_framebuffer;
	rt->target->renderBuf.clearColor = 0xFFFFFFFF;
	
	current_env = p_env.is_valid() ? environment_owner.get(p_env) : NULL;
// 	current_vd=viewport_data_owner.get(p_viewport_data);
	
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_PRIMARY_COLOR, GPU_FRAGMENT_SECONDARY_COLOR, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_ADD);
	
	C3D_TexBind(0, NULL);

	C3D_BindProgram(&scene_shader->program);
	_set_uniform(scene_shader->location_modelview, Matrix32());
	_set_uniform(scene_shader->location_extra, Matrix32());
};

void RasterizerCitro3d::begin_shadow_map( RID p_light_instance, int p_shadow_pass ) {

}

void RasterizerCitro3d::set_camera(const Transform& p_world, const CameraMatrix& p_projection, bool p_ortho_hint)
{

	
	print("set_camera\n");
	camera_transform=p_world;
// 	if (current_rt && current_rt_vflip) {
	if (current_rt_vflip) {
// 		m[4] = -1;
// 		camera_transform.basis.set_axis(1,-camera_transform.basis.get_axis(1));
	}
// 	camera_transform.basis.set_axis(0,-camera_transform.basis.get_axis(0));
	
	camera_transform_inverse=camera_transform.inverse();
	camera_projection = p_projection;
// 	camera_projection.get_fov()
// 	camera_projection=*reinterpret_cast<CameraMatrix*>(m) * p_projection;
// 	camera_projection = camera_projection * m;
// 	camera_projection.invert();
	camera_plane = Plane( camera_transform.origin, -camera_transform.basis.get_axis(2) );
	camera_z_near=camera_projection.get_z_near();
	camera_z_far=camera_projection.get_z_far();
	camera_projection.get_viewport_size(camera_vp_size.x,camera_vp_size.y);
	camera_ortho=p_ortho_hint;
	
	print("z-near:%f z-far:%f\n", camera_z_near, camera_z_far);
	print("x:%f y:%f\n", camera_vp_size.x,camera_vp_size.y);
	
// 	for (int i = 0; i < 4; ++i)
// 		print("%f %f %f %f\n", m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]);

	_set_uniform(scene_shader->location_modelview, Matrix32());
// 	_set_uniform(scene_shader->location_worldTransform, Matrix32());
	_set_uniform(scene_shader->location_worldTransform, camera_transform_inverse);
	_set_uniform(scene_shader->location_projection, camera_projection);
		
// 	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, scene_shader->location_projection, &mtx);
}

void RasterizerCitro3d::add_light( RID p_light_instance )
{


}

void RasterizerCitro3d::add_immediate( const RID& p_immediate, const InstanceData *p_data)
{
	print("add_immediate\n");
}

void RasterizerCitro3d::add_mesh( const RID& p_mesh, const InstanceData *p_data)
{
	print("add_mesh\n");
	Mesh *mesh = mesh_owner.get(p_mesh);
	ERR_FAIL_COND(!mesh);

	int ssize = mesh->surfaces.size();

	for (int i=0;i<ssize;i++)
	{
		int mat_idx = p_data->materials[i].is_valid() ? i : -1;
		Surface *s = mesh->surfaces[i];
		_add_geometry(s,p_data,s,NULL,mat_idx);
	}

// 	mesh->last_pass=frame;
}

void RasterizerCitro3d::add_multimesh( const RID& p_multimesh, const InstanceData *p_data)
{
	print("add_multimesh\n");
}

void RasterizerCitro3d::add_particles( const RID& p_particle_instance, const InstanceData *p_data)
{
	ParticlesInstance *particles_instance = particles_instance_owner.get(p_particle_instance);
	ERR_FAIL_COND(!particles_instance);
	Particles *p=particles_owner.get( particles_instance->particles );
	ERR_FAIL_COND(!p);

	_add_geometry(p, p_data, p, particles_instance);
// 	draw_next_frame=true;
}



void RasterizerCitro3d::end_scene()
{
	print("end_scene\n");
	
// 	opaque_render_list.sort_mat_light_type_flags();
	_render_list_forward(&opaque_render_list, camera_transform, camera_transform_inverse,camera_projection,false,fragment_lighting);
	
// 	alpha_render_list.sort_z();
// 	_render_list_forward(&alpha_render_list, camera_transform, camera_transform_inverse,camera_projection,false,fragment_lighting,true);
	
	C3D_Flush();
}
void RasterizerCitro3d::end_shadow_map() {

}


void RasterizerCitro3d::end_frame()
{
// 	C3D_FrameEnd(0);
	
	print("end_frame %d %f\n", vertexArrays.size(), C3D_GetCmdBufUsage());
	
	RenderTarget* rt = current_rt ? current_rt : base_framebuffer;
	
	C3D_Flush();
	C3D_RenderBufTransfer(&rt->target->renderBuf, (u32*)gfxGetFramebuffer(rt->target->screen, rt->target->side, NULL, NULL), rt->target->transferFlags);
	
	C3D_RenderBufClear(&rt->target->renderBuf);
	
	VertexArray **varray = vertexArrays.ptr();
	for (int i = 0; i < vertexArrays.size(); ++i)
		memdelete(*varray++);
	vertexArrays.clear();
	
	OS::get_singleton()->swap_buffers();
}

void RasterizerCitro3d::flush_frame()
{
	C3D_Flush();
}

RID RasterizerCitro3d::canvas_light_occluder_create()
{
	CanvasOccluder *co = memnew( CanvasOccluder );
	co->index_id=0;
	co->vertex_id=0;
	co->len=0;

	return canvas_occluder_owner.make_rid(co);
}

void RasterizerCitro3d::canvas_light_occluder_set_polylines(RID p_occluder, const DVector<Vector2>& p_lines)
{
	CanvasOccluder *co = canvas_occluder_owner.get(p_occluder);
	ERR_FAIL_COND(!co);
	return;

	co->lines=p_lines;

	if (p_lines.size()!=co->len) {

// 		if (co->index_id)
// 			glDeleteBuffers(1,&co->index_id);
// 		if (co->vertex_id)
// 			glDeleteBuffers(1,&co->vertex_id);

		co->index_id=0;
		co->vertex_id=0;
		co->len=0;

	}

	if (p_lines.size()) {



		DVector<float> geometry;
		DVector<uint16_t> indices;
		int lc = p_lines.size();

		geometry.resize(lc*6);
		indices.resize(lc*3);

		DVector<float>::Write vw=geometry.write();
		DVector<uint16_t>::Write iw=indices.write();


		DVector<Vector2>::Read lr=p_lines.read();

		const int POLY_HEIGHT = 16384;

		for(int i=0;i<lc/2;i++) {

			vw[i*12+0]=lr[i*2+0].x;
			vw[i*12+1]=lr[i*2+0].y;
			vw[i*12+2]=POLY_HEIGHT;

			vw[i*12+3]=lr[i*2+1].x;
			vw[i*12+4]=lr[i*2+1].y;
			vw[i*12+5]=POLY_HEIGHT;

			vw[i*12+6]=lr[i*2+1].x;
			vw[i*12+7]=lr[i*2+1].y;
			vw[i*12+8]=-POLY_HEIGHT;

			vw[i*12+9]=lr[i*2+0].x;
			vw[i*12+10]=lr[i*2+0].y;
			vw[i*12+11]=-POLY_HEIGHT;

			iw[i*6+0]=i*4+0;
			iw[i*6+1]=i*4+1;
			iw[i*6+2]=i*4+2;

			iw[i*6+3]=i*4+2;
			iw[i*6+4]=i*4+3;
			iw[i*6+5]=i*4+0;

		}

		//if same buffer len is being set, just use BufferSubData to avoid a pipeline flush


		if (!co->vertex_id) {
// 			glGenBuffers(1,&co->vertex_id);
// 			glBindBuffer(GL_ARRAY_BUFFER,co->vertex_id);
// 			glBufferData(GL_ARRAY_BUFFER,lc*6*sizeof(real_t),vw.ptr(),GL_STATIC_DRAW);
		} else {

// 			glBindBuffer(GL_ARRAY_BUFFER,co->vertex_id);
// 			glBufferSubData(GL_ARRAY_BUFFER,0,lc*6*sizeof(real_t),vw.ptr());

		}

// 		glBindBuffer(GL_ARRAY_BUFFER,0); //unbind

		if (!co->index_id) {

// 			glGenBuffers(1,&co->index_id);
// 			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,co->index_id);
// 			glBufferData(GL_ELEMENT_ARRAY_BUFFER,lc*3*sizeof(uint16_t),iw.ptr(),GL_STATIC_DRAW);
		} else {


// 			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,co->index_id);
// 			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,lc*3*sizeof(uint16_t),iw.ptr());
		}

// 		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0); //unbind

		co->len=lc;
	}
}

RID RasterizerCitro3d::canvas_light_shadow_buffer_create(int p_width)
{
	return RID();
	CanvasLightShadow *cls = memnew( CanvasLightShadow );
	if (p_width>1024)
		p_width=1024;

	cls->size=p_width;
	cls->height=16;

/*
	glActiveTexture(GL_TEXTURE0);

	glGenFramebuffers(1, &cls->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, cls->fbo);

	// Create a render buffer
	glGenRenderbuffers(1, &cls->rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, cls->rbo);

	// Create a texture for storing the depth
	glGenTextures(1, &cls->depth);
	glBindTexture(GL_TEXTURE_2D, cls->depth);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Remove artifact on the edges of the shadowmap
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
*/
	cls->renderTarget = C3D_RenderTargetCreate(480, 800, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(cls->renderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetClear(cls->renderTarget, C3D_CLEAR_ALL, 0x0, 0);
	
// 	ERR_FAIL_COND(!C3D_TexInit(&cls->texture, w, h, GPU_RGBA8));


/*
	//print_line("ERROR? "+itos(glGetError()));
	if ( read_depth_supported ) {

		// We'll use a depth texture to store the depths in the shadow map
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, cls->size, cls->height, 0,
			     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

#ifdef GLEW_ENABLED
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif

		// Attach the depth texture to FBO depth attachment point
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				       GL_TEXTURE_2D, cls->depth, 0);

#ifdef GLEW_ENABLED
		glDrawBuffer(GL_NONE);
#endif

	} else {
		// We'll use a RGBA texture into which we pack the depth info
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cls->size, cls->height, 0,
			     GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		// Attach the RGBA texture to FBO color attachment point
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D, cls->depth, 0);
		cls->rgba=cls->depth;

		// Allocate 16-bit depth buffer
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, cls->size, cls->height);

		// Attach the render buffer as depth buffer - will be ignored
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					  GL_RENDERBUFFER, cls->rbo);


	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	//printf("errnum: %x\n",status);
#ifdef GLEW_ENABLED
	if (read_depth_supported) {
		//glDrawBuffer(GL_BACK);
	}
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, base_framebuffer);
	DEBUG_TEST_ERROR("2D Shadow Buffer Init");
	ERR_FAIL_COND_V( status != GL_FRAMEBUFFER_COMPLETE, RID() );

#ifdef GLEW_ENABLED
	if (read_depth_supported) {
		//glDrawBuffer(GL_BACK);
	}
#endif
*/
	return canvas_light_shadow_owner.make_rid(cls);
}

void RasterizerCitro3d::canvas_light_shadow_buffer_update(RID p_buffer, const Matrix32& p_light_xform, int p_light_mask,float p_near, float p_far, CanvasLightOccluderInstance* p_occluders, CameraMatrix *p_xform_cache)
{
	return;
}

void RasterizerCitro3d::canvas_debug_viewport_shadows(CanvasLight* p_lights_with_shadow) {


}

/* CANVAS API */


void RasterizerCitro3d::begin_canvas_bg()
{
	print("begin_canvas_bg\n");
}
void RasterizerCitro3d::canvas_begin()
{
	print("canvas_begin");
	
	C3D_Mtx projection;
	Mtx_OrthoTilt(&projection, 0.0, 800.0, 480.0, 0.0, 0.0, 1.0, true);
	
	C3D_BindProgram(&canvas_shader->program);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, canvas_shader->location_projection, &projection);
	
	_set_uniform(canvas_shader->location_modelview, Matrix32());
	_set_uniform(canvas_shader->location_extra, Matrix32());
	
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord
	AttrInfo_AddFixed(attrInfo, 2); // v2=color
	
	canvas_opacity = 1.0;
	
	canvas_blend_mode = VS::MATERIAL_BLEND_MODE_MIX;
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

	print(" %f\n", C3D_GetCmdBufUsage());
}
void RasterizerCitro3d::canvas_disable_blending()
{
	print("canvas_disable_blending\n");
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

void RasterizerCitro3d::canvas_set_opacity(float p_opacity)
{
	canvas_opacity = p_opacity;
}

void RasterizerCitro3d::canvas_set_blend_mode(VS::MaterialBlendMode p_mode)
{
	if (p_mode==canvas_blend_mode)
		return;

	switch(p_mode)
	{
		 case VS::MATERIAL_BLEND_MODE_MIX: {
			if (current_rt && current_rt_transparent) {
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			}
			else {
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
			}

		 } break;
		 case VS::MATERIAL_BLEND_MODE_ADD: {

			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);

		 } break;
		 case VS::MATERIAL_BLEND_MODE_SUB: {
			C3D_AlphaBlend(GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_REVERSE_SUBTRACT, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
		 } break;
		case VS::MATERIAL_BLEND_MODE_MUL: {
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ZERO, GPU_DST_COLOR, GPU_ZERO);
		} break;
		case VS::MATERIAL_BLEND_MODE_PREMULT_ALPHA: {
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
		} break;
	}

	canvas_blend_mode=p_mode;
}

void RasterizerCitro3d::canvas_begin_rect(const Matrix32& p_transform)
{
	_set_uniform(canvas_shader->location_modelview, p_transform);
	_set_uniform(canvas_shader->location_extra, Matrix32());
// 	canvas_transform = p_transform;
}

void RasterizerCitro3d::canvas_set_clip(bool p_clip, const Rect2& p_rect) {




}

void RasterizerCitro3d::canvas_end_rect()
{
	print("canvas_end_rect\n");
}

void RasterizerCitro3d::canvas_draw_line(const Point2& p_from, const Point2& p_to, const Color& p_color, float p_width)
{
	print("canvas_draw_line\n");
}

void RasterizerCitro3d::canvas_draw_rect(const Rect2& p_rect, int p_flags, const Rect2& p_source,RID p_texture,const Color& p_modulate)
{
// 	print("canvas_draw_rect %.1f %.1f %.1f %.1f\n", p_rect.pos.x, p_rect.pos.y, p_rect.size.width, p_rect.size.height);
	
	// Set the fixed attribute (color)
	C3D_FixedAttribSet(2, p_modulate.r, p_modulate.g, p_modulate.b, p_modulate.a * canvas_opacity);
// 	C3D_FixedAttribSet(2, 1.f, 1.f, 1.f, 1.f);
	
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	
	
	if (p_texture.is_valid())
	{
		Texture* texture = texture_owner.get(p_texture);
// 		ERR_FAIL_COND(!texture);
		
		if (p_flags & CANVAS_RECT_TILE && !(texture->flags & VS::TEXTURE_FLAG_REPEAT))
		{
			
		}
		
		C3D_TexBind(0, &texture->tex);
		
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
		
		if (!(p_flags&CANVAS_RECT_REGION)) {

			Rect2 region = Rect2(0,0,texture->tex.width,texture->tex.height);
			_draw_textured_quad(p_rect,region,region.size,p_flags&CANVAS_RECT_FLIP_H,p_flags&CANVAS_RECT_FLIP_V,p_flags&CANVAS_RECT_TRANSPOSE);

		} else {

			_draw_textured_quad(p_rect, p_source, Size2(texture->tex.width,texture->tex.height),p_flags&CANVAS_RECT_FLIP_H,p_flags&CANVAS_RECT_FLIP_V,p_flags&CANVAS_RECT_TRANSPOSE);

		}
	}
	else
	{
		C3D_TexBind(0, NULL);
		
		C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
		
		_draw_quad(p_rect);
	}
}
void RasterizerCitro3d::canvas_draw_style_box(const Rect2& p_rect, const Rect2& p_src_region, RID p_texture,const float *p_margin, bool p_draw_center,const Color& p_modulate)
{
	print("canvas_draw_style_box\n");
	
// 	Color m = p_modulate;
// 	m.a*=canvas_opacity;
// 	_set_color_attrib(m);
	C3D_FixedAttribSet(2, p_modulate.r, p_modulate.g, p_modulate.b, p_modulate.a * canvas_opacity);

	Texture* texture=_bind_texture(p_texture);
	ERR_FAIL_COND(!texture);

	Rect2 region = p_src_region;
	if (region.size.width <= 0 )
	    region.size.width = texture->width;
	if (region.size.height <= 0)
	    region.size.height = texture->height;
	/* CORNERS */
	_draw_textured_quad( // top left
		Rect2( p_rect.pos, Size2(p_margin[MARGIN_LEFT],p_margin[MARGIN_TOP])),
		Rect2( region.pos, Size2(p_margin[MARGIN_LEFT],p_margin[MARGIN_TOP])),
		Size2( texture->width, texture->height ) );

	_draw_textured_quad( // top right
		Rect2( Point2( p_rect.pos.x + p_rect.size.width - p_margin[MARGIN_RIGHT], p_rect.pos.y), Size2(p_margin[MARGIN_RIGHT],p_margin[MARGIN_TOP])),
		Rect2( Point2(region.pos.x+region.size.width-p_margin[MARGIN_RIGHT], region.pos.y), Size2(p_margin[MARGIN_RIGHT],p_margin[MARGIN_TOP])),
		Size2( texture->width, texture->height ) );


	_draw_textured_quad( // bottom left
		Rect2( Point2(p_rect.pos.x,p_rect.pos.y + p_rect.size.height - p_margin[MARGIN_BOTTOM]), Size2(p_margin[MARGIN_LEFT],p_margin[MARGIN_BOTTOM])),
		Rect2( Point2(region.pos.x, region.pos.y+region.size.height-p_margin[MARGIN_BOTTOM]), Size2(p_margin[MARGIN_LEFT],p_margin[MARGIN_BOTTOM])),
		Size2( texture->width, texture->height ) );

	_draw_textured_quad( // bottom right
		Rect2( Point2( p_rect.pos.x + p_rect.size.width - p_margin[MARGIN_RIGHT], p_rect.pos.y + p_rect.size.height - p_margin[MARGIN_BOTTOM]), Size2(p_margin[MARGIN_RIGHT],p_margin[MARGIN_BOTTOM])),
		Rect2( Point2(region.pos.x+region.size.width-p_margin[MARGIN_RIGHT], region.pos.y+region.size.height-p_margin[MARGIN_BOTTOM]), Size2(p_margin[MARGIN_RIGHT],p_margin[MARGIN_BOTTOM])),
		Size2( texture->width, texture->height ) );

	Rect2 rect_center( p_rect.pos+Point2( p_margin[MARGIN_LEFT], p_margin[MARGIN_TOP]), Size2( p_rect.size.width - p_margin[MARGIN_LEFT] - p_margin[MARGIN_RIGHT], p_rect.size.height - p_margin[MARGIN_TOP] - p_margin[MARGIN_BOTTOM] ));

	Rect2 src_center( Point2(region.pos.x+p_margin[MARGIN_LEFT], region.pos.y+p_margin[MARGIN_TOP]), Size2(region.size.width - p_margin[MARGIN_LEFT] - p_margin[MARGIN_RIGHT], region.size.height - p_margin[MARGIN_TOP] - p_margin[MARGIN_BOTTOM] ));


	_draw_textured_quad( // top
		Rect2( Point2(rect_center.pos.x,p_rect.pos.y),Size2(rect_center.size.width,p_margin[MARGIN_TOP])),
		Rect2( Point2(src_center.pos.x,region.pos.y), Size2(src_center.size.width,p_margin[MARGIN_TOP])),
		Size2( texture->width, texture->height ) );

	_draw_textured_quad( // bottom
		Rect2( Point2(rect_center.pos.x,rect_center.pos.y+rect_center.size.height),Size2(rect_center.size.width,p_margin[MARGIN_BOTTOM])),
		Rect2( Point2(src_center.pos.x,src_center.pos.y+src_center.size.height), Size2(src_center.size.width,p_margin[MARGIN_BOTTOM])),
		Size2( texture->width, texture->height ) );

	_draw_textured_quad( // left
		Rect2( Point2(p_rect.pos.x,rect_center.pos.y),Size2(p_margin[MARGIN_LEFT],rect_center.size.height)),
		Rect2( Point2(region.pos.x,region.pos.y+p_margin[MARGIN_TOP]), Size2(p_margin[MARGIN_LEFT],src_center.size.height)),
		Size2( texture->width, texture->height ) );

	_draw_textured_quad( // right
		Rect2( Point2(rect_center.pos.x+rect_center.size.width,rect_center.pos.y),Size2(p_margin[MARGIN_RIGHT],rect_center.size.height)),
		Rect2( Point2(src_center.pos.x+src_center.size.width,region.pos.y+p_margin[MARGIN_TOP]), Size2(p_margin[MARGIN_RIGHT],src_center.size.height)),
		Size2( texture->width, texture->height ) );

	if (p_draw_center) {
		_draw_textured_quad(
			rect_center,
			src_center,
			Size2( texture->width, texture->height ));
	}

	_rinfo.ci_draw_commands++;
}
void RasterizerCitro3d::canvas_draw_primitive(const Vector<Point2>& p_points, const Vector<Color>& p_colors,const Vector<Point2>& p_uvs, RID p_texture,float p_width)
{
	print("canvas_draw_primitive\n");
}


void RasterizerCitro3d::canvas_draw_polygon(int p_vertex_count, const int* p_indices, const Vector2* p_vertices, const Vector2* p_uvs, const Color* p_colors,const RID& p_texture,bool p_singlecolor)
{
	print("canvas_draw_polygon\n");
}

RasterizerCitro3d::Texture* RasterizerCitro3d::_bind_texture(const RID& p_texture)
{
	if (p_texture.is_valid())
	{
		Texture*texture=texture_owner.get(p_texture);
		C3D_TexBind(0, &texture->tex);
		return texture;
	}
	
	C3D_TexBind(0, NULL);
	return NULL;
}

void RasterizerCitro3d::_set_uniform(int uniform_location, const Matrix32& p_transform)
{
	const Matrix32& tr = p_transform;
	
	C3D_Mtx mtx;
// 	float matrix[16] = { /* build a 16x16 matrix */
// 		tr.elements[2][0], tr.elements[2][1], 0, 1,
// 		0, 0, 1, 0,
// 		tr.elements[1][0], tr.elements[1][1], 0, 0,
// 		tr.elements[0][0], tr.elements[0][1], 0, 0,
// 	};
	
// 	float m[16]={ /* build a 16x16 matrix */
// 		tr.elements[0][0], tr.elements[0][1], 0, 0,
// 		tr.elements[1][0], tr.elements[1][1], 0, 0,
// 		0, 0, 1, 0,
// 		tr.elements[2][0], tr.elements[2][1], 0, 1
// 	};
	
	float matrix[16] = { /* build a 16x16 matrix */
		tr.elements[2][0], 0, tr.elements[1][0], tr.elements[0][0],
		tr.elements[2][1], 0, tr.elements[1][1], tr.elements[0][1],
		0, 1, 0, 0,
		1, 0, 0, 0,
	};
	
	memcpy(mtx.m, matrix, sizeof(matrix));
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_location, &mtx);
}

void RasterizerCitro3d::_set_uniform(int uniform_location, const Transform& p_transform)
{
	const Transform& tr = p_transform;
	
	C3D_Mtx mtx;
// 	float matrix[16]={ /* build a 16x16 matrix */
// 		tr.basis.elements[0][0], tr.basis.elements[1][0], tr.basis.elements[2][0], 0,
// 		tr.basis.elements[0][1], tr.basis.elements[1][1], tr.basis.elements[2][1], 0,
// 		tr.basis.elements[0][2], tr.basis.elements[1][2], tr.basis.elements[2][2], 0,
// 		tr.origin.x, tr.origin.y, tr.origin.z, 1
// 	};
	float matrix[16]={ /* build a 16x16 matrix */
		tr.origin.x, tr.basis.elements[0][2], tr.basis.elements[0][1], tr.basis.elements[0][0],
		tr.origin.y, tr.basis.elements[1][2], tr.basis.elements[1][1], tr.basis.elements[1][0],
		tr.origin.z, tr.basis.elements[2][2], tr.basis.elements[2][1], tr.basis.elements[2][0],
		1, 0, 0, 0,
	};
	
	memcpy(mtx.m, matrix, sizeof(matrix));
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_location, &mtx);
}

void RasterizerCitro3d::_set_uniform(int uniform_location, const CameraMatrix& p_matrix)
{
	// Swap X/Y axis due to 3DS screens being sideways
	// and adjust clipping range from [-1, 1] to [-1, 0]
	float m[16] = {
		0, -1, 0, 0,
		1, 0, 0, 0,
		0, 0, 0.5, 0,
// 		0, 0, 1, 0,
		0, 0, -0.5, 1,
	};
	
	CameraMatrix p = *reinterpret_cast<CameraMatrix*>(m) * p_matrix;
	
	C3D_Mtx mtx;
	float *to = mtx.m;
	const float *from = reinterpret_cast<const float*>(p.matrix);
// 	for (int i = 0; i < 4; ++i)
// 	{
// 		mtx.r[i].x = *from++;
// 		mtx.r[i].y = *from++;
// 		mtx.r[i].z = *from++;
// 		mtx.r[i].w = *from++;
// 	}
	
	to[0] = from[12];
	to[1] = from[8];
	to[2] = from[4];
	to[3] = from[0];
	to[4] = from[13];
	to[5] = from[9];
	to[6] = from[5];
	to[7] = from[1];
	to[8] = from[14];
	to[9] = from[10];
	to[10] = from[6];
	to[11] = from[2];
	to[12] = from[15];
	to[13] = from[11];
	to[14] = from[7];
	to[15] = from[3];
	
// 	for (int i = 0; i < 4; ++i)
// 		print("%f %f %f %f\n", to[i*4], to[i*4+1], to[i*4+2], to[i*4+3]);
	
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_location, &mtx);
}

void RasterizerCitro3d::canvas_set_transform(const Matrix32& p_transform)
{
	_set_uniform(canvas_shader->location_extra, p_transform);
}

static void _set_scissor(int x, int y, int w, int h)
{
	print("set_scissor %d %d %d %d\n", x, y, w, h);
	// Keep in mind the sideway 3ds screen, so it seems screwy
	int bottom = 400 - x;
	int top = 240 - y;
	int left = top - h;
	int right = bottom - w;
	if (bottom < 0) bottom = 0;
	if (top < 0) top = 0;
	if (left < 0) left = 0;
	if (right < 0) right = 0;
	C3D_SetScissor(GPU_SCISSOR_NORMAL, left, right, top, bottom);
// 	C3D_SetScissor(GPU_SCISSOR_NORMAL, 240 - (y + h), 400 - (x + w), 240 - y, 400 - x);
}

void RasterizerCitro3d::_set_canvas_scissor(CanvasItem* p_item)
{
	int x, y, w, h;

	if (current_rt) {
		x = p_item->final_clip_rect.pos.x;
		y = p_item->final_clip_rect.pos.y;
		w = p_item->final_clip_rect.size.x;
		h = p_item->final_clip_rect.size.y;
	}
	else
	{
		x = p_item->final_clip_rect.pos.x;
		y = 240 - (p_item->final_clip_rect.pos.y + p_item->final_clip_rect.size.y);
		w = p_item->final_clip_rect.size.x;
		h = p_item->final_clip_rect.size.y;
	}
	
	_set_scissor(x, y, w, h);
}

template<bool use_normalmap>
void RasterizerCitro3d::_canvas_item_render_commands(CanvasItem *p_item,CanvasItem *current_clip,bool &reclip)
{
	int cc=p_item->commands.size();
	CanvasItem::Command **commands = p_item->commands.ptr();
	
// 	print("_canvas_item_render_commands\n");

	for(int i=0;i<cc;i++) {

		CanvasItem::Command *c=commands[i];

		switch(c->type) {
			case CanvasItem::Command::TYPE_LINE: {

				CanvasItem::CommandLine* line = static_cast<CanvasItem::CommandLine*>(c);
				canvas_draw_line(line->from,line->to,line->color,line->width);
			} break;
			case CanvasItem::Command::TYPE_RECT: {

				CanvasItem::CommandRect* rect = static_cast<CanvasItem::CommandRect*>(c);
//						canvas_draw_rect(rect->rect,rect->region,rect->source,rect->flags&CanvasItem::CommandRect::FLAG_TILE,rect->flags&CanvasItem::CommandRect::FLAG_FLIP_H,rect->flags&CanvasItem::CommandRect::FLAG_FLIP_V,rect->texture,rect->modulate);
#if 0
				int flags=0;

				if (rect->flags&CanvasItem::CommandRect::FLAG_REGION) {
					flags|=Rasterizer::CANVAS_RECT_REGION;
				}
				if (rect->flags&CanvasItem::CommandRect::FLAG_TILE) {
					flags|=Rasterizer::CANVAS_RECT_TILE;
				}
				if (rect->flags&CanvasItem::CommandRect::FLAG_FLIP_H) {

					flags|=Rasterizer::CANVAS_RECT_FLIP_H;
				}
				if (rect->flags&CanvasItem::CommandRect::FLAG_FLIP_V) {

					flags|=Rasterizer::CANVAS_RECT_FLIP_V;
				}
#else

				int flags=rect->flags;
#endif
// 				if (use_normalmap)
// 					_canvas_normal_set_flip(Vector2((flags&CANVAS_RECT_FLIP_H)?-1:1,(flags&CANVAS_RECT_FLIP_V)?-1:1));
				canvas_draw_rect(rect->rect,flags,rect->source,rect->texture,rect->modulate);

			} break;
			case CanvasItem::Command::TYPE_STYLE: {

				CanvasItem::CommandStyle* style = static_cast<CanvasItem::CommandStyle*>(c);
// 				if (use_normalmap)
// 					_canvas_normal_set_flip(Vector2(1,1));
				canvas_draw_style_box(style->rect,style->source,style->texture,style->margin,style->draw_center,style->color);

			} break;
			case CanvasItem::Command::TYPE_PRIMITIVE: {

// 				if (use_normalmap)
// 					_canvas_normal_set_flip(Vector2(1,1));
				CanvasItem::CommandPrimitive* primitive = static_cast<CanvasItem::CommandPrimitive*>(c);
				canvas_draw_primitive(primitive->points,primitive->colors,primitive->uvs,primitive->texture,primitive->width);
			} break;
			case CanvasItem::Command::TYPE_POLYGON: {

// 				if (use_normalmap)
// 					_canvas_normal_set_flip(Vector2(1,1));
				CanvasItem::CommandPolygon* polygon = static_cast<CanvasItem::CommandPolygon*>(c);
				canvas_draw_polygon(polygon->count,polygon->indices.ptr(),polygon->points.ptr(),polygon->uvs.ptr(),polygon->colors.ptr(),polygon->texture,polygon->colors.size()==1);

			} break;

			case CanvasItem::Command::TYPE_POLYGON_PTR: {

// 				if (use_normalmap)
// 					_canvas_normal_set_flip(Vector2(1,1));
				CanvasItem::CommandPolygonPtr* polygon = static_cast<CanvasItem::CommandPolygonPtr*>(c);
				canvas_draw_polygon(polygon->count,polygon->indices,polygon->points,polygon->uvs,polygon->colors,polygon->texture,false);
			} break;
			case CanvasItem::Command::TYPE_CIRCLE: {

				CanvasItem::CommandCircle* circle = static_cast<CanvasItem::CommandCircle*>(c);
				static const int numpoints=32;
				Vector2 points[numpoints+1];
				points[numpoints]=circle->pos;
				int indices[numpoints*3];

				for(int i=0;i<numpoints;i++) {

					points[i]=circle->pos+Vector2( Math::sin(i*Math_PI*2.0/numpoints),Math::cos(i*Math_PI*2.0/numpoints) )*circle->radius;
					indices[i*3+0]=i;
					indices[i*3+1]=(i+1)%numpoints;
					indices[i*3+2]=numpoints;
				}
				canvas_draw_polygon(numpoints*3,indices,points,NULL,&circle->color,RID(),true);
				//canvas_draw_circle(circle->indices.size(),circle->indices.ptr(),circle->points.ptr(),circle->uvs.ptr(),circle->colors.ptr(),circle->texture,circle->colors.size()==1);
			} break;
			case CanvasItem::Command::TYPE_TRANSFORM: {
				CanvasItem::CommandTransform* transform = static_cast<CanvasItem::CommandTransform*>(c);
				canvas_set_transform(transform->xform);
			} break;
			case CanvasItem::Command::TYPE_BLEND_MODE: {

				CanvasItem::CommandBlendMode* bm = static_cast<CanvasItem::CommandBlendMode*>(c);
				canvas_set_blend_mode(bm->blend_mode);

			} break;
			case CanvasItem::Command::TYPE_CLIP_IGNORE: {

				CanvasItem::CommandClipIgnore* ci = static_cast<CanvasItem::CommandClipIgnore*>(c);
				if (current_clip) {

					if (ci->ignore!=reclip) {
						if (ci->ignore) {
							C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
							reclip = true;
						} else {							
							_set_canvas_scissor(current_clip);
							reclip=false;
						}
					}
				}
			} break;
		}
	}

}

void RasterizerCitro3d::canvas_render_items(CanvasItem *p_item_list,int p_z,const Color& p_modulate,CanvasLight *p_light)
{
// 	print("canvas_render_items Z:%d\n", p_z);
	
	canvas_opacity=1.0;
// 	canvas_use_modulate=p_modulate!=Color(1,1,1,1);
// 	canvas_modulate=p_modulate;
	
// 	C3D_FixedAttribSet(2, p_modulate.r, p_modulate.g, p_modulate.b, canvas_opacity);
// 	C3D_FixedAttribSet(2, 1.f, 1.f, 1.f, 1.f);
	
	CanvasItem *current_clip = NULL;
	
	while(p_item_list)
	{
		CanvasItem *ci = p_item_list;
		
		// Handle clipping (scissor test)
		if (current_clip != ci->final_clip_owner)
		{
			current_clip = ci->final_clip_owner;
			if (current_clip)
			{
// 				int x, y, w, h;
// 				
// 				if (current_rt) {
// 					x = current_clip->final_clip_rect.pos.x;
// 					y = current_clip->final_clip_rect.pos.y;
// 					w = current_clip->final_clip_rect.size.x;
// 					h = current_clip->final_clip_rect.size.y;
// 				}
// 				else {
// 					x = current_clip->final_clip_rect.pos.x;
// 					y = 240 - (current_clip->final_clip_rect.pos.y + current_clip->final_clip_rect.size.y);
// 					w = current_clip->final_clip_rect.size.x;
// 					h = current_clip->final_clip_rect.size.y;
// 				}
// 				_set_scissor(x, y, w, h);
				_set_canvas_scissor(current_clip);
			}
			else
				C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
		}
		
		// Handle material/shader
		CanvasItem *material_owner = ci->material_owner?ci->material_owner:ci;
		CanvasItemMaterial *material = material_owner->material;
		
		_set_uniform(canvas_shader->location_modelview,ci->final_transform);
		_set_uniform(canvas_shader->location_extra, Matrix32());
		
		bool unshaded = (material && material->shading_mode==VS::CANVAS_ITEM_SHADING_UNSHADED) || ci->blend_mode!=VS::MATERIAL_BLEND_MODE_MIX;
		
		bool reclip=false;
		
		if (ci==p_item_list || ci->blend_mode!=canvas_blend_mode)
			canvas_set_blend_mode(ci->blend_mode);

		canvas_opacity = ci->final_opacity;
		
		if (unshaded || (p_modulate.a>0.001 && (!material || material->shading_mode!=VS::CANVAS_ITEM_SHADING_ONLY_LIGHT) && !ci->light_masked ))
			_canvas_item_render_commands<false>(ci, current_clip, reclip);
		
		if (reclip)
		{
			_set_canvas_scissor(current_clip);
		}
		
		p_item_list = p_item_list->next;
	}
	
	if (current_clip)
		C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

/* ENVIRONMENT */

RID RasterizerCitro3d::environment_create()
{
	print("environment_create\n");
	Environment * env = memnew( Environment );
	return environment_owner.make_rid(env);
}

void RasterizerCitro3d::environment_set_background(RID p_env,VS::EnvironmentBG p_bg)
{
	ERR_FAIL_INDEX(p_bg,VS::ENV_BG_MAX);
	Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->bg_mode=p_bg;
}

VS::EnvironmentBG RasterizerCitro3d::environment_get_background(RID p_env) const{

	const Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env,VS::ENV_BG_MAX);
	return env->bg_mode;
}

void RasterizerCitro3d::environment_set_background_param(RID p_env,VS::EnvironmentBGParam p_param, const Variant& p_value){

	ERR_FAIL_INDEX(p_param,VS::ENV_BG_PARAM_MAX);
	Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->bg_param[p_param]=p_value;

}
Variant RasterizerCitro3d::environment_get_background_param(RID p_env,VS::EnvironmentBGParam p_param) const{

	ERR_FAIL_INDEX_V(p_param,VS::ENV_BG_PARAM_MAX,Variant());
	const Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env,Variant());
	return env->bg_param[p_param];

}

void RasterizerCitro3d::environment_set_enable_fx(RID p_env,VS::EnvironmentFx p_effect,bool p_enabled){

	ERR_FAIL_INDEX(p_effect,VS::ENV_FX_MAX);
	Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->fx_enabled[p_effect]=p_enabled;
}
bool RasterizerCitro3d::environment_is_fx_enabled(RID p_env,VS::EnvironmentFx p_effect) const{

	ERR_FAIL_INDEX_V(p_effect,VS::ENV_FX_MAX,false);
	const Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env,false);
	return env->fx_enabled[p_effect];

}

void RasterizerCitro3d::environment_fx_set_param(RID p_env,VS::EnvironmentFxParam p_param,const Variant& p_value){

	ERR_FAIL_INDEX(p_param,VS::ENV_FX_PARAM_MAX);
	Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND(!env);
	env->fx_param[p_param]=p_value;
}
Variant RasterizerCitro3d::environment_fx_get_param(RID p_env,VS::EnvironmentFxParam p_param) const{

	ERR_FAIL_INDEX_V(p_param,VS::ENV_FX_PARAM_MAX,Variant());
	const Environment * env = environment_owner.get(p_env);
	ERR_FAIL_COND_V(!env,Variant());
	return env->fx_param[p_param];

}


RID RasterizerCitro3d::sampled_light_dp_create(int p_width,int p_height) {

	return sampled_light_owner.make_rid(memnew(SampledLight));
}

void RasterizerCitro3d::sampled_light_dp_update(RID p_sampled_light, const Color *p_data, float p_multiplier) {


}


/*MISC*/

bool RasterizerCitro3d::is_texture(const RID& p_rid) const {

	return texture_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_material(const RID& p_rid) const {

	return material_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_mesh(const RID& p_rid) const {

	return mesh_owner.owns(p_rid);
}

bool RasterizerCitro3d::is_immediate(const RID& p_rid) const {

	return immediate_owner.owns(p_rid);
}

bool RasterizerCitro3d::is_multimesh(const RID& p_rid) const {

	return multimesh_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_particles(const RID &p_beam) const {

	return particles_owner.owns(p_beam);
}

bool RasterizerCitro3d::is_light(const RID& p_rid) const {

	return light_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_light_instance(const RID& p_rid) const {

	return light_instance_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_particles_instance(const RID& p_rid) const {

	return particles_instance_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_skeleton(const RID& p_rid) const {

	return skeleton_owner.owns(p_rid);
}
bool RasterizerCitro3d::is_environment(const RID& p_rid) const {

	return environment_owner.owns(p_rid);
}

bool RasterizerCitro3d::is_canvas_light_occluder(const RID& p_rid) const {

	return canvas_occluder_owner.owns(p_rid);
}

bool RasterizerCitro3d::is_shader(const RID& p_rid) const {

	return shader_owner.owns(p_rid);
}

void RasterizerCitro3d::free(const RID& p_rid) {

	if (texture_owner.owns(p_rid)) {

// 		print("delete texture\n");
		Texture *texture = texture_owner.get(p_rid);
		texture_owner.free(p_rid);
		memdelete(texture);

	} else if (shader_owner.owns(p_rid)) {

		print("delete shader\n");
		Shader *shader = shader_owner.get(p_rid);
		shader_owner.free(p_rid);
		memdelete(shader);

	} else if (material_owner.owns(p_rid)) {

		Material *material = material_owner.get( p_rid );
		material_owner.free(p_rid);
		memdelete(material);

	} else if (mesh_owner.owns(p_rid)) {

		Mesh *mesh = mesh_owner.get(p_rid);

		for (int i=0;i<mesh->surfaces.size();i++) {
			Surface *surface = mesh->surfaces[i];
			if (surface->array_local)
				linearFree(surface->array_local);
			if (surface->index_array_local)
				linearFree(surface->index_array_local);
			
			if (mesh->morph_target_count>0) {

				for(int i=0;i<mesh->morph_target_count;i++) {

					memdelete_arr(surface->morph_targets_local[i].array);
				}
				memdelete_arr(surface->morph_targets_local);
				surface->morph_targets_local=NULL;
			}
			
			memdelete( mesh->surfaces[i] );
		};

		mesh->surfaces.clear();
		mesh_owner.free(p_rid);
		memdelete(mesh);

	} else if (multimesh_owner.owns(p_rid)) {

	       MultiMesh *multimesh = multimesh_owner.get(p_rid);
	       multimesh_owner.free(p_rid);
	       memdelete(multimesh);

	} else if (immediate_owner.owns(p_rid)) {

		Immediate *immediate = immediate_owner.get(p_rid);
		immediate_owner.free(p_rid);
		memdelete(immediate);

	} else if (particles_owner.owns(p_rid)) {

		Particles *particles = particles_owner.get(p_rid);
		particles_owner.free(p_rid);
		memdelete(particles);
	} else if (particles_instance_owner.owns(p_rid)) {

		ParticlesInstance *particles_isntance = particles_instance_owner.get(p_rid);
		particles_instance_owner.free(p_rid);
		memdelete(particles_isntance);

	} else if (skeleton_owner.owns(p_rid)) {

		Skeleton *skeleton = skeleton_owner.get( p_rid );
		skeleton_owner.free(p_rid);
		memdelete(skeleton);

	} else if (light_owner.owns(p_rid)) {

		Light *light = light_owner.get( p_rid );
		light_owner.free(p_rid);
		memdelete(light);

	} else if (light_instance_owner.owns(p_rid)) {

		LightInstance *light_instance = light_instance_owner.get( p_rid );
		light_instance_owner.free(p_rid);
		memdelete( light_instance );

	} else if (canvas_occluder_owner.owns(p_rid)) {
		
		CanvasOccluder *co = canvas_occluder_owner.get(p_rid);
		
		canvas_occluder_owner.free(p_rid);
		memdelete(co);
		
	} else if (canvas_light_shadow_owner.owns(p_rid)) {
		
		CanvasLightShadow *cls = canvas_light_shadow_owner.get(p_rid);
		C3D_RenderTargetDelete(cls->renderTarget);
		
		canvas_light_shadow_owner.free(p_rid);
		memdelete(cls);

	} else if (environment_owner.owns(p_rid)) {

		Environment *env = environment_owner.get( p_rid );
		environment_owner.free(p_rid);
		memdelete( env );
	} else if (sampled_light_owner.owns(p_rid)) {

		SampledLight *sampled_light = sampled_light_owner.get( p_rid );
		ERR_FAIL_COND(!sampled_light);

		sampled_light_owner.free(p_rid);
		memdelete( sampled_light );

	};
}


void RasterizerCitro3d::custom_shade_model_set_shader(int p_model, RID p_shader) {


};

RID RasterizerCitro3d::custom_shade_model_get_shader(int p_model) const {

	return RID();
};

void RasterizerCitro3d::custom_shade_model_set_name(int p_model, const String& p_name) {

};

String RasterizerCitro3d::custom_shade_model_get_name(int p_model) const {

	return String();
};

void RasterizerCitro3d::custom_shade_model_set_param_info(int p_model, const List<PropertyInfo>& p_info) {

};

void RasterizerCitro3d::custom_shade_model_get_param_info(int p_model, List<PropertyInfo>* p_info) const {

};

void RasterizerCitro3d::_render_list_forward(RenderList *p_render_list,const Transform& p_view_transform, const Transform& p_view_transform_inverse,const CameraMatrix& p_projection,bool p_reverse_cull,bool p_fragment_light,bool p_alpha_pass)
{
	print("_render_list_forward\n");
	
	if (current_rt && current_rt_vflip) {
		//p_reverse_cull=!p_reverse_cull;
// 		glFrontFace(GL_CCW);

	}

	const Material *prev_material=NULL;
	uint16_t prev_light=0x777E;
	const Geometry *prev_geometry_cmp=NULL;
	uint8_t prev_light_type=0xEF;
	const Skeleton *prev_skeleton =NULL;
	uint8_t prev_sort_flags=0xFF;
	const BakedLightData *prev_baked_light=NULL;
	RID prev_baked_light_texture;
	const float *prev_morph_values=NULL;
	int prev_receive_shadows_state=-1;
	
	
/*
	material_shader.set_conditional(MaterialShaderGLES2::USE_VERTEX_LIGHTING,!shadow && !p_fragment_light);
	material_shader.set_conditional(MaterialShaderGLES2::USE_FRAGMENT_LIGHTING,!shadow && p_fragment_light);
	material_shader.set_conditional(MaterialShaderGLES2::USE_SKELETON,false);

	if (shadow) {
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_DIRECTIONAL,false);
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_OMNI,false);
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_SPOT,false);
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_SHADOW,false);
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM,false);
		material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM4,false);
		material_shader.set_conditional(MaterialShaderGLES2::SHADELESS,false);
		material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_OCTREE,false);
		material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_LIGHTMAP,false);
//		material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_TEXTURE,false);

	}
*/


// 	bool stores_glow = !shadow && (current_env && current_env->fx_enabled[VS::ENV_FX_GLOW]) && !p_alpha_pass;
	float sampled_light_dp_multiplier=1.0;

	bool prev_blend=false;
// 	glDisable(GL_BLEND);
	
	for (int i=0;i<p_render_list->element_count;i++) {

		RenderList::Element *e = p_render_list->elements[i];
		const Material *material = e->material;
		uint16_t light = e->light;
		uint8_t light_type = e->light_type;
		uint8_t sort_flags= e->sort_flags;
		const Skeleton *skeleton = e->skeleton;
		const Geometry *geometry_cmp = e->geometry_cmp;
		const BakedLightData *baked_light = e->instance->baked_light;
		const float *morph_values = e->instance->morph_values.ptr();
		int receive_shadows_state = e->instance->receive_shadows == true ? 1 : 0;

		bool rebind=false;
		bool bind_baked_light_octree=false;
		bool bind_baked_lightmap=false;
		bool additive=false;
		bool bind_dp_sampler=false;

/*
		if (!shadow) {

			if (texscreen_used && !texscreen_copied && material->shader_cache && material->shader_cache->valid && material->shader_cache->has_texscreen) {
				texscreen_copied=true;
				_copy_to_texscreen();

				//force reset state
				prev_material=NULL;
				prev_light=0x777E;
				prev_geometry_cmp=NULL;
				prev_light_type=0xEF;
				prev_skeleton =NULL;
				prev_sort_flags=0xFF;
				prev_morph_values=NULL;
				prev_receive_shadows_state=-1;
				glEnable(GL_BLEND);
				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);
				glDisable(GL_SCISSOR_TEST);

			}

			if (light_type!=prev_light_type || receive_shadows_state!=prev_receive_shadows_state) {

				if (material->flags[VS::MATERIAL_FLAG_UNSHADED] || current_debug==VS::SCENARIO_DEBUG_SHADELESS) {
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_DIRECTIONAL,false);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_OMNI,false);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_SPOT,false);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_SHADOW,false);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM,false);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM4,false);
					material_shader.set_conditional(MaterialShaderGLES2::SHADELESS,true);
				} else {
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_DIRECTIONAL,(light_type&0x3)==VS::LIGHT_DIRECTIONAL);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_OMNI,(light_type&0x3)==VS::LIGHT_OMNI);
					material_shader.set_conditional(MaterialShaderGLES2::LIGHT_TYPE_SPOT,(light_type&0x3)==VS::LIGHT_SPOT);
					if (receive_shadows_state==1) {
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_SHADOW,(light_type&0x8));
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM,(light_type&0x10));
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM4,(light_type&0x20));
					}
					else {
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_SHADOW,false);
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM,false);
						material_shader.set_conditional(MaterialShaderGLES2::LIGHT_USE_PSSM4,false);
					}
					material_shader.set_conditional(MaterialShaderGLES2::SHADELESS,false);
				}

				rebind=true;
			}


			if (!*e->additive_ptr) {

				additive=false;
				*e->additive_ptr=true;
			} else {
				additive=true;
			}


			if (stores_glow)
				material_shader.set_conditional(MaterialShaderGLES2::USE_GLOW,!additive);


			bool desired_blend=false;
			VS::MaterialBlendMode desired_blend_mode=VS::MATERIAL_BLEND_MODE_MIX;

			if (additive) {
				desired_blend=true;
				desired_blend_mode=VS::MATERIAL_BLEND_MODE_ADD;
			} else {
				desired_blend=p_alpha_pass;
				desired_blend_mode=material->blend_mode;
			}

			if (prev_blend!=desired_blend) {

				if (desired_blend) {
					glEnable(GL_BLEND);
					if (!current_rt || !current_rt_transparent)
						glColorMask(1,1,1,0);
				} else {
					glDisable(GL_BLEND);
					glColorMask(1,1,1,1);

				}

				prev_blend=desired_blend;
			}

			if (desired_blend && desired_blend_mode!=current_blend_mode) {


				switch(desired_blend_mode) {

					 case VS::MATERIAL_BLEND_MODE_MIX: {
						glBlendEquation(GL_FUNC_ADD);
						if (current_rt && current_rt_transparent) {
// 							glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
						}
						else {
// 							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						}

					 } break;
					 case VS::MATERIAL_BLEND_MODE_ADD: {

// 						glBlendEquation(GL_FUNC_ADD);
// 						glBlendFunc(p_alpha_pass?GL_SRC_ALPHA:GL_ONE,GL_ONE);

					 } break;
					 case VS::MATERIAL_BLEND_MODE_SUB: {

// 						glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
// 						glBlendFunc(GL_SRC_ALPHA,GL_ONE);
					 } break;
					case VS::MATERIAL_BLEND_MODE_MUL: {
						glBlendEquation(GL_FUNC_ADD);
						if (current_rt && current_rt_transparent) {
// 							glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
						}
						else {
// 							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						}

					} break;

				}

				current_blend_mode=desired_blend_mode;
			}

			material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_OCTREE,false);
			material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_LIGHTMAP,false);
			material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_DP_SAMPLER,false);

			material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_COLOR, false);


			if (material->flags[VS::MATERIAL_FLAG_UNSHADED] == false && current_debug != VS::SCENARIO_DEBUG_SHADELESS) {

				if (baked_light != NULL) {
					if (baked_light->realtime_color_enabled) {
						float realtime_energy = baked_light->realtime_energy;
						material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_COLOR, true);
						material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_COLOR, Vector3(baked_light->realtime_color.r*realtime_energy, baked_light->realtime_color.g*realtime_energy, baked_light->realtime_color.b*realtime_energy));
					}
				}

				if (e->instance->sampled_light.is_valid()) {

					SampledLight *sl = sampled_light_owner.get(e->instance->sampled_light);
					if (sl) {

						baked_light = NULL; //can't mix
						material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_DP_SAMPLER, true);
						glActiveTexture(GL_TEXTURE0 + max_texture_units - 3);
						glBindTexture(GL_TEXTURE_2D, sl->texture); //bind the texture
						sampled_light_dp_multiplier = sl->multiplier;
						bind_dp_sampler = true;
					}
				}


				if (!additive && baked_light) {

					if (baked_light->mode == VS::BAKED_LIGHT_OCTREE && baked_light->octree_texture.is_valid() && e->instance->baked_light_octree_xform) {
						material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_OCTREE, true);
						bind_baked_light_octree = true;
						if (prev_baked_light != baked_light) {
							Texture *tex = texture_owner.get(baked_light->octree_texture);
							if (tex) {

								glActiveTexture(GL_TEXTURE0 + max_texture_units - 3);
								glBindTexture(tex->target, tex->tex_id); //bind the texture
							}
							if (baked_light->light_texture.is_valid()) {
								Texture *texl = texture_owner.get(baked_light->light_texture);
								if (texl) {
									glActiveTexture(GL_TEXTURE0 + max_texture_units - 4);
									glBindTexture(texl->target, texl->tex_id); //bind the light texture
								}
							}

						}
					}
					else if (baked_light->mode == VS::BAKED_LIGHT_LIGHTMAPS) {


						int lightmap_idx = e->instance->baked_lightmap_id;

						material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_LIGHTMAP, false);
						bind_baked_lightmap = false;


						if (baked_light->lightmaps.has(lightmap_idx)) {


							RID texid = baked_light->lightmaps[lightmap_idx];

							if (prev_baked_light != baked_light || texid != prev_baked_light_texture) {


								Texture *tex = texture_owner.get(texid);
								if (tex) {

									glActiveTexture(GL_TEXTURE0 + max_texture_units - 3);
									glBindTexture(tex->target, tex->tex_id); //bind the texture
								}

								prev_baked_light_texture = texid;
							}

							if (texid.is_valid()) {
								material_shader.set_conditional(MaterialShaderGLES2::ENABLE_AMBIENT_LIGHTMAP, true);
								bind_baked_lightmap = true;
							}

						}
					}
				}

				if (int(prev_baked_light != NULL) ^ int(baked_light != NULL)) {
					rebind = true;
				}
			}
		} // !shadow

		if (sort_flags!=prev_sort_flags) {

			if (sort_flags&RenderList::SORT_FLAG_INSTANCING) {
				material_shader.set_conditional(MaterialShaderGLES2::USE_UNIFORM_INSTANCING,!use_texture_instancing && !use_attribute_instancing);
				material_shader.set_conditional(MaterialShaderGLES2::USE_ATTRIBUTE_INSTANCING,use_attribute_instancing);
				material_shader.set_conditional(MaterialShaderGLES2::USE_TEXTURE_INSTANCING,use_texture_instancing);
			} else {
				material_shader.set_conditional(MaterialShaderGLES2::USE_UNIFORM_INSTANCING,false);
				material_shader.set_conditional(MaterialShaderGLES2::USE_ATTRIBUTE_INSTANCING,false);
				material_shader.set_conditional(MaterialShaderGLES2::USE_TEXTURE_INSTANCING,false);
			}
			rebind=true;
		}

		if  (use_hw_skeleton_xform && (skeleton!=prev_skeleton||morph_values!=prev_morph_values)) {
			if (!prev_skeleton || !skeleton)
				rebind=true; //went from skeleton <-> no skeleton, needs rebind

			if (morph_values==NULL)
				_setup_skeleton(skeleton);
			else
				_setup_skeleton(NULL);
		}
*/
		if (material!=prev_material || rebind) {

			rebind = _setup_material(e->geometry,material,additive,!p_alpha_pass);

			print("Setup material\n");
			_rinfo.mat_change_count++;
		} else {

			if (prev_skeleton!=skeleton) {
				//_setup_material_skeleton(material,skeleton);
			};
		}


		if (geometry_cmp!=prev_geometry_cmp || prev_skeleton!=skeleton) {

			_setup_geometry(e->geometry, material,e->skeleton,e->instance->morph_values.ptr());
			_rinfo.surface_count++;
// 			DEBUG_TEST_ERROR("Setup geometry");
		};

		if (i==0 || light!=prev_light || rebind) {
			if (e->light!=0xFFFF) {
				_setup_light(e->light);
			}
// 			_setup_lights(e->lights,e->light_count);
		}
/*
		if (bind_baked_light_octree && (baked_light!=prev_baked_light || rebind)) {

			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_INVERSE_TRANSFORM, *e->instance->baked_light_octree_xform);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LATTICE_SIZE, baked_light->octree_lattice_size);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LATTICE_DIVIDE, baked_light->octree_lattice_divide);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_STEPS, baked_light->octree_steps);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_TEX,max_texture_units-3);
			if (baked_light->light_texture.is_valid()) {

				material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LIGHT_TEX,max_texture_units-4);
				material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LIGHT_PIX_SIZE,baked_light->light_tex_pixel_size);
			} else {
				material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LIGHT_TEX,max_texture_units-3);
				material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_LIGHT_PIX_SIZE,baked_light->octree_tex_pixel_size);
			}
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_MULTIPLIER,baked_light->texture_multiplier);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_OCTREE_PIX_SIZE,baked_light->octree_tex_pixel_size);


		}

		if (bind_baked_lightmap && (baked_light!=prev_baked_light || rebind)) {

			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_LIGHTMAP, max_texture_units-3);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_LIGHTMAP_MULTIPLIER, baked_light->lightmap_multiplier);

		}

		if (bind_dp_sampler) {

			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_DP_SAMPLER_MULTIPLIER,sampled_light_dp_multiplier);
			material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_DP_SAMPLER,max_texture_units-3);
		}

		_set_cull(e->mirror,p_reverse_cull);


		if (i==0 || rebind) {
			material_shader.set_uniform(MaterialShaderGLES2::CAMERA_INVERSE_TRANSFORM, p_view_transform_inverse);
			material_shader.set_uniform(MaterialShaderGLES2::PROJECTION_TRANSFORM, p_projection);
			if (!shadow) {

				if (!additive && current_env && current_env->fx_enabled[VS::ENV_FX_AMBIENT_LIGHT]) {
					Color ambcolor = _convert_color(current_env->fx_param[VS::ENV_FX_PARAM_AMBIENT_LIGHT_COLOR]);
					float ambnrg =  current_env->fx_param[VS::ENV_FX_PARAM_AMBIENT_LIGHT_ENERGY];
					material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_LIGHT,Vector3(ambcolor.r*ambnrg,ambcolor.g*ambnrg,ambcolor.b*ambnrg));
				} else {
					material_shader.set_uniform(MaterialShaderGLES2::AMBIENT_LIGHT,Vector3());
				}
			}

			_rinfo.shader_change_count++;
		}

		if (skeleton != prev_skeleton || rebind) {
			if (skeleton && morph_values == NULL) {
				material_shader.set_uniform(MaterialShaderGLES2::SKELETON_MATRICES, max_texture_units - 2);
				material_shader.set_uniform(MaterialShaderGLES2::SKELTEX_PIXEL_SIZE, skeleton->pixel_size);
			}
		}
*/
// 		if (e->instance->billboard || e->instance->billboard_y || e->instance->depth_scale) {
		if (false) {
			print("billboard\n");
			Transform xf=e->instance->transform;
			if (e->instance->depth_scale) {

				if (p_projection.matrix[3][3]) {
					//orthogonal matrix, try to do about the same
					//with viewport size
					//real_t w = Math::abs( 1.0/(2.0*(p_projection.matrix[0][0])) );
					real_t h = Math::abs( 1.0/(2.0*p_projection.matrix[1][1]) );
					float sc = (h*2.0); //consistent with Y-fov
					xf.basis.scale( Vector3(sc,sc,sc));
				} else {
					//just scale by depth
					real_t sc = -camera_plane.distance_to(xf.origin);
					xf.basis.scale( Vector3(sc,sc,sc));
				}
			}

			if (e->instance->billboard) {

				Vector3 scale = xf.basis.get_scale();

				if (current_rt && current_rt_vflip) {
					xf.set_look_at(xf.origin, xf.origin + p_view_transform.get_basis().get_axis(2), -p_view_transform.get_basis().get_axis(1));
				} else {
					xf.set_look_at(xf.origin, xf.origin + p_view_transform.get_basis().get_axis(2), p_view_transform.get_basis().get_axis(1));
				}

				xf.basis.scale(scale);
			}
			
			if (e->instance->billboard_y) {
				
				Vector3 scale = xf.basis.get_scale();
				Vector3 look_at =  p_view_transform.get_origin();
				look_at.y = 0.0;
				Vector3 look_at_norm = look_at.normalized();
				
				if (current_rt && current_rt_vflip) {
					xf.set_look_at(xf.origin,xf.origin + look_at_norm, Vector3(0.0, -1.0, 0.0));
				} else {
					xf.set_look_at(xf.origin,xf.origin + look_at_norm, Vector3(0.0, 1.0, 0.0));
				}
				xf.basis.scale(scale);
			}
// 			material_shader.set_uniform(MaterialShaderGLES2::WORLD_TRANSFORM, xf);

		} else {
// 			Transform tr = camera_transform_inverse * e->instance->transform;
			_set_uniform(scene_shader->location_modelview, e->instance->transform);
// 			material_shader.set_uniform(MaterialShaderGLES2::WORLD_TRANSFORM, e->instance->transform);
		}

// 		material_shader.set_uniform(MaterialShaderGLES2::NORMAL_MULT, e->mirror?-1.0:1.0);
// 		material_shader.set_uniform(MaterialShaderGLES2::CONST_LIGHT_MULT,additive?0.0:1.0);


		_render(e->geometry, material, skeleton,e->owner,e->instance->transform);
// 		DEBUG_TEST_ERROR("Rendering");

		prev_material=material;
		prev_skeleton=skeleton;
		prev_geometry_cmp=geometry_cmp;
		prev_light=e->light;
		prev_light_type=e->light_type;
		prev_sort_flags=sort_flags;
		prev_baked_light=baked_light;
		prev_morph_values=morph_values;
		prev_receive_shadows_state=receive_shadows_state;
	}

	//print_line("shaderchanges: "+itos(p_alpha_pass)+": "+itos(_rinfo.shader_change_count));


	if (current_rt && current_rt_vflip) {
// 		glFrontFace(GL_CW);
	}

};

bool RasterizerCitro3d::_setup_material(const Geometry *p_geometry,const Material *p_material,bool p_no_const_light,bool p_opaque_pass)
{
	Texture *texture = NULL;

	if (p_material->flags[VS::MATERIAL_FLAG_DOUBLE_SIDED]) {
// 		glDisable(GL_CULL_FACE);
		C3D_CullFace(GPU_CULL_NONE);
		
	} else {
// 		glEnable(GL_CULL_FACE);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
	}
	
// 	C3D_TexEnv* env = C3D_GetTexEnv(0);
// 	C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_PRIMARY_COLOR, GPU_FRAGMENT_SECONDARY_COLOR, 0);
// 	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
// 	C3D_TexEnvFunc(env, C3D_Both, GPU_ADD);
	
	C3D_TexEnv *env = C3D_GetTexEnv(1);

// 	if (p_material->line_width)
// 		glLineWidth(p_material->line_width);
/*
	//all goes to false by default
	material_shader.set_conditional(MaterialShaderGLES2::USE_SHADOW_PASS,shadow!=NULL);
	material_shader.set_conditional(MaterialShaderGLES2::USE_SHADOW_PCF,shadow_filter==SHADOW_FILTER_PCF5 || shadow_filter==SHADOW_FILTER_PCF13);
	material_shader.set_conditional(MaterialShaderGLES2::USE_SHADOW_PCF_HQ,shadow_filter==SHADOW_FILTER_PCF13);
	material_shader.set_conditional(MaterialShaderGLES2::USE_SHADOW_ESM,shadow_filter==SHADOW_FILTER_ESM);
	material_shader.set_conditional(MaterialShaderGLES2::USE_LIGHTMAP_ON_UV2,p_material->flags[VS::MATERIAL_FLAG_LIGHTMAP_ON_UV2]);
	material_shader.set_conditional(MaterialShaderGLES2::USE_COLOR_ATTRIB_SRGB_TO_LINEAR,p_material->flags[VS::MATERIAL_FLAG_COLOR_ARRAY_SRGB] && current_env && current_env->fx_enabled[VS::ENV_FX_SRGB]);

	if (p_opaque_pass && p_material->depth_draw_mode==VS::MATERIAL_DEPTH_DRAW_OPAQUE_PRE_PASS_ALPHA && p_material->shader_cache && p_material->shader_cache->has_alpha) {

		material_shader.set_conditional(MaterialShaderGLES2::ENABLE_CLIP_ALPHA,true);
	} else {
		material_shader.set_conditional(MaterialShaderGLES2::ENABLE_CLIP_ALPHA,false);

	}
*/

// 	if (!shadow) {

		bool depth_test=!p_material->flags[VS::MATERIAL_FLAG_ONTOP];
		bool depth_write=p_material->depth_draw_mode!=VS::MATERIAL_DEPTH_DRAW_NEVER && (p_opaque_pass || p_material->depth_draw_mode==VS::MATERIAL_DEPTH_DRAW_ALWAYS);
		//bool depth_write=!p_material->hints[VS::MATERIAL_HINT_NO_DEPTH_DRAW] && (p_opaque_pass || !p_material->hints[VS::MATERIAL_HINT_NO_DEPTH_DRAW_FOR_ALPHA]);

		if (current_depth_mask!=depth_write)
		{
			current_depth_mask=depth_write;
// 			glDepthMask( depth_write );
		}


		if (current_depth_test!=depth_test)
		{
			current_depth_test=depth_test;
// 			if(depth_test)
// 				glEnable(GL_DEPTH_TEST);
// 			else
// 				glDisable(GL_DEPTH_TEST);
		}


// 		material_shader.set_conditional(MaterialShaderGLES2::USE_FOG,current_env && current_env->fx_enabled[VS::ENV_FX_FOG]);
// 	}


// 	DEBUG_TEST_ERROR("Pre Shader Bind");

	bool rebind=false;

// 	if (p_material->shader_cache && p_material->shader_cache->valid) {

	//	// reduce amount of conditional compilations
	//	for(int i=0;i<_tex_version_count;i++)
	//		material_shader.set_conditional((MaterialShaderGLES2::Conditionals)_tex_version[i],false);


	//	material_shader.set_custom_shader(p_material->shader_cache->custom_code_id);

// 		if (p_material->shader_version!=p_material->shader_cache->version) {
			//shader changed somehow, must update uniforms

// 			_update_material_shader_params((Material*)p_material);

// 		}
// 		material_shader.set_custom_shader(p_material->shader_cache->custom_code_id);
// 		rebind = material_shader.bind();

// 		DEBUG_TEST_ERROR("Shader Bind");

		//set uniforms!
		int texcoord=0;
		for (Map<StringName,Material::UniformData>::Element *E=p_material->shader_params.front();E;E=E->next())
		{

// 			if (E->get().index<0)
// 				continue;
			
			if (!texture && E->get().istexture) {
				//clearly a texture..
				RID rid = E->get().value;
				print("rid:%u\n", rid.get_id());
// 				int loc = material_shader.get_custom_uniform_location(E->get().index); //should be automatic..

				texture=_bind_texture(rid);
// 				if (rid.is_valid()) {
// 					texture=texture_owner.get(rid);
// 					if (!texture) {
// 						E->get().value=RID(); //nullify, invalid texture
// 						rid=RID();
// 					}
// 				}

// 				glActiveTexture(GL_TEXTURE0+texcoord);
// 				glUniform1i(loc,texcoord); //TODO - this could happen automatically on compile...
				texcoord++;

			} else if (E->get().value.get_type()==Variant::COLOR){
				Color c = E->get().value;
// 				material_shader.set_custom_uniform(E->get().index,_convert_color(c));
			} else {
// 				material_shader.set_custom_uniform(E->get().index,E->get().value);
			}

		}
		
		print("texture count: %d\n", texcoord);
		
		if (texture)
		{
			print("binding material texture\n");
			C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE0, 0);
			C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
			C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
		}
		else
		{
			print("unbinding material texture\n");
			C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, 0, 0);
			C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
			C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
		}


// 		if (p_material->shader_cache->has_texscreen && framebuffer.active) {
// 			material_shader.set_uniform(MaterialShaderGLES2::TEXSCREEN_SCREEN_MULT,Vector2(float(viewport.width)/framebuffer.width,float(viewport.height)/framebuffer.height));
// 			material_shader.set_uniform(MaterialShaderGLES2::TEXSCREEN_SCREEN_CLAMP,Color(0,0,float(viewport.width)/framebuffer.width,float(viewport.height)/framebuffer.height));
// 			material_shader.set_uniform(MaterialShaderGLES2::TEXSCREEN_TEX,texcoord);
// 			glActiveTexture(GL_TEXTURE0+texcoord);
// 			glBindTexture(GL_TEXTURE_2D,framebuffer.sample_color);
// 			C3D_TexBind(texcoord, NULL);

// 		}
// 		if (p_material->shader_cache->has_screen_uv) {
// 			material_shader.set_uniform(MaterialShaderGLES2::SCREEN_UV_MULT,Vector2(1.0/viewport.width,1.0/viewport.height));
// 		}
// 		DEBUG_TEST_ERROR("Material parameters");

// 		if (p_material->shader_cache->uses_time) {
// 			material_shader.set_uniform(MaterialShaderGLES2::TIME,Math::fmod(last_time,shader_time_rollback));
// 			draw_next_frame=true;
// 		}
			//if uses TIME - draw_next_frame=true


// 	} else {

// 		material_shader.set_custom_shader(0);
// 		rebind = material_shader.bind();

// 		DEBUG_TEST_ERROR("Shader bind2");
// 	}


/*
	if (shadow) {

		float zofs = shadow->base->vars[VS::LIGHT_PARAM_SHADOW_Z_OFFSET];
		float zslope = shadow->base->vars[VS::LIGHT_PARAM_SHADOW_Z_SLOPE_SCALE];
		if (shadow_pass>=1 && shadow->base->type==VS::LIGHT_DIRECTIONAL) {
			float m = Math::pow(shadow->base->directional_shadow_param[VS::LIGHT_DIRECTIONAL_SHADOW_PARAM_PSSM_ZOFFSET_SCALE],shadow_pass);
			zofs*=m;
			zslope*=m;
		}
		material_shader.set_uniform(MaterialShaderGLES2::SHADOW_Z_OFFSET,zofs);
		material_shader.set_uniform(MaterialShaderGLES2::SHADOW_Z_SLOPE_SCALE,zslope);
		if (shadow->base->type==VS::LIGHT_OMNI)
			material_shader.set_uniform(MaterialShaderGLES2::DUAL_PARABOLOID,shadow->dp);
		DEBUG_TEST_ERROR("Shadow uniforms");

	}


	if (current_env && current_env->fx_enabled[VS::ENV_FX_FOG]) {

		Color col_begin = current_env->fx_param[VS::ENV_FX_PARAM_FOG_BEGIN_COLOR];
		Color col_end = current_env->fx_param[VS::ENV_FX_PARAM_FOG_END_COLOR];
		col_begin=_convert_color(col_begin);
		col_end=_convert_color(col_end);
		float from = current_env->fx_param[VS::ENV_FX_PARAM_FOG_BEGIN];
		float zf = camera_z_far;
		float curve = current_env->fx_param[VS::ENV_FX_PARAM_FOG_ATTENUATION];
		material_shader.set_uniform(MaterialShaderGLES2::FOG_PARAMS,Vector3(from,zf,curve));
		material_shader.set_uniform(MaterialShaderGLES2::FOG_COLOR_BEGIN,Vector3(col_begin.r,col_begin.g,col_begin.b));
		material_shader.set_uniform(MaterialShaderGLES2::FOG_COLOR_END,Vector3(col_end.r,col_end.g,col_end.b));
	}
*/


	//material_shader.set_uniform(MaterialShaderGLES2::TIME,Math::fmod(last_time,300.0));
	//if uses TIME - draw_next_frame=true

	return rebind;
}

void RasterizerCitro3d::_setup_light(uint16_t p_light)
{
	
}

Error RasterizerCitro3d::_setup_geometry(const Geometry *p_geometry, const Material* p_material, const Skeleton *p_skeleton,const float *p_morphs)
{
}

void RasterizerCitro3d::_add_geometry( const Geometry* p_geometry, const InstanceData *p_instance, const Geometry *p_geometry_cmp, const GeometryOwner *p_owner,int p_material)
{
	Material *m=NULL;
	RID m_src;
	
	if (p_instance->material_override.is_valid()) {
// 		print("material override\n");
		m_src = p_instance->material_override;
	}else if (p_material >= 0)
		m_src = p_instance->materials[p_material];
	else
		m_src = p_geometry->material;

// #ifdef DEBUG_ENABLED
// 	if (current_debug==VS::SCENARIO_DEBUG_OVERDRAW) {
// 		m_src=overdraw_material;
// 	}
// 
// #endif

	if (m_src)
		m=material_owner.get( m_src );

	if (!m) {
		print("using default material\n");
		m=material_owner.get( default_material );
	}

	ERR_FAIL_COND(!m);
/*
	if (m->last_pass!=frame) {

		if (m->shader.is_valid()) {

			m->shader_cache=shader_owner.get(m->shader);
			if (m->shader_cache) {



				if (!m->shader_cache->valid) {
					m->shader_cache=NULL;
				} else {
					if (m->shader_cache->has_texscreen)
						texscreen_used=true;
				}
			} else {
				m->shader=RID();
			}

		} else {
			m->shader_cache=NULL;
		}

		m->last_pass=frame;
	}
*/


	RenderList *render_list=NULL;

// 	bool has_base_alpha=(m->shader_cache && m->shader_cache->has_alpha);
// 	bool has_blend_alpha=m->blend_mode!=VS::MATERIAL_BLEND_MODE_MIX || m->flags[VS::MATERIAL_FLAG_ONTOP];
// 	bool has_alpha = has_base_alpha || has_blend_alpha;

/*
	if (shadow) {

		if (has_blend_alpha || (has_base_alpha && m->depth_draw_mode!=VS::MATERIAL_DEPTH_DRAW_OPAQUE_PRE_PASS_ALPHA))
			return; //bye

		if (!m->shader_cache || (!m->shader_cache->writes_vertex && !m->shader_cache->uses_discard && m->depth_draw_mode!=VS::MATERIAL_DEPTH_DRAW_OPAQUE_PRE_PASS_ALPHA)) {
			//shader does not use discard and does not write a vertex position, use generic material
			if (p_instance->cast_shadows == VS::SHADOW_CASTING_SETTING_DOUBLE_SIDED)
				m = shadow_mat_double_sided_ptr;
			else
				m = shadow_mat_ptr;
			if (m->last_pass!=frame) {

				if (m->shader.is_valid()) {

					m->shader_cache=shader_owner.get(m->shader);
					if (m->shader_cache) {


						if (!m->shader_cache->valid)
							m->shader_cache=NULL;
					} else {
						m->shader=RID();
					}

				} else {
					m->shader_cache=NULL;
				}

				m->last_pass=frame;
			}
		}

		render_list = &opaque_render_list;

	}
	else
	{
		if (has_alpha) {
			render_list = &alpha_render_list;
		} else {
			render_list = &opaque_render_list;

		}
	}
*/
	render_list = &opaque_render_list;

	RenderList::Element *e = render_list->add_element();

	if (!e)
		return;

	e->geometry=p_geometry;
	e->geometry_cmp=p_geometry_cmp;
	e->material=m;
	e->instance=p_instance;
// 	if (camera_ortho) {
// 		e->depth=camera_plane.distance_to(p_instance->transform.origin);
// 	} else {
// 		e->depth=camera_transform.origin.distance_to(p_instance->transform.origin);
// 	}
	e->depth = 0;
	
	e->owner=p_owner;
	e->light_type=0;
	e->additive=false;
	e->additive_ptr=&e->additive;
	e->sort_flags=0;


	if (p_instance->skeleton.is_valid()) {
		e->skeleton=skeleton_owner.get(p_instance->skeleton);
		if (!e->skeleton)
			const_cast<InstanceData*>(p_instance)->skeleton=RID();
		else
			e->sort_flags|=RenderList::SORT_FLAG_SKELETON;
	} else {
		e->skeleton=NULL;
	}

	if (e->geometry->type==Geometry::GEOMETRY_MULTISURFACE)
		e->sort_flags|=RenderList::SORT_FLAG_INSTANCING;


	e->mirror=p_instance->mirror;
	if (m->flags[VS::MATERIAL_FLAG_INVERT_FACES])
		e->mirror=!e->mirror;

	e->light_type=0xFF; // no lights!
	e->light_type=3; //light type 3 is no light?
	e->light=0xFFFF;
	
/*

	if (!shadow && !has_blend_alpha && has_alpha && m->depth_draw_mode==VS::MATERIAL_DEPTH_DRAW_OPAQUE_PRE_PASS_ALPHA) {

		//if nothing exists, add this element as opaque too
		RenderList::Element *oe = opaque_render_list.add_element();

		if (!oe)
			return;

		memcpy(oe,e,sizeof(RenderList::Element));
		oe->additive_ptr=&oe->additive;
	}

	if (shadow || m->flags[VS::MATERIAL_FLAG_UNSHADED] || current_debug==VS::SCENARIO_DEBUG_SHADELESS) {

		e->light_type=0x7F; //unshaded is zero
	} else {

		bool duplicate=false;


		for(int i=0;i<directional_light_count;i++) {
			uint16_t sort_key = directional_lights[i]->sort_key;
			uint8_t light_type = VS::LIGHT_DIRECTIONAL;
			if (directional_lights[i]->base->shadow_enabled) {
				light_type|=0x8;
				if (directional_lights[i]->base->directional_shadow_mode==VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS)
					light_type|=0x10;
				else if (directional_lights[i]->base->directional_shadow_mode==VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS)
					light_type|=0x30;

			}

			RenderList::Element *ec;
			if (duplicate) {

				ec = render_list->add_element();
				memcpy(ec,e,sizeof(RenderList::Element));
			} else {

				ec=e;
				duplicate=true;
			}

			ec->light_type=light_type;
			ec->light=sort_key;
			ec->additive_ptr=&e->additive;

		}


		const RID *liptr = p_instance->light_instances.ptr();
		int ilc=p_instance->light_instances.size();



		for(int i=0;i<ilc;i++) {

			LightInstance *li=light_instance_owner.get( liptr[i] );
			if (!li || li->last_pass!=scene_pass) //lit by light not in visible scene
				continue;
			uint8_t light_type=li->base->type|0x40; //penalty to ensure directionals always go first
			if (li->base->shadow_enabled) {
				light_type|=0x8;
			}
			uint16_t sort_key =li->sort_key;

			RenderList::Element *ec;
			if (duplicate) {

				ec = render_list->add_element();
				memcpy(ec,e,sizeof(RenderList::Element));
			} else {

				duplicate=true;
				ec=e;
			}

			ec->light_type=light_type;
			ec->light=sort_key;
			ec->additive_ptr=&e->additive;

		}
	}
	
	*/

// 	DEBUG_TEST_ERROR("Add Geometry");
}

static const GPU_Primitive_t gl_primitive[]={
	GPU_TRIANGLES,
	GPU_TRIANGLES,
	GPU_TRIANGLES,
	GPU_TRIANGLES,
	GPU_TRIANGLES,
	GPU_TRIANGLE_STRIP,
	GPU_TRIANGLE_FAN
};

void RasterizerCitro3d::_render(const Geometry *p_geometry,const Material *p_material, const Skeleton* p_skeleton, const GeometryOwner *p_owner,const Transform& p_xform)
{
	_rinfo.object_count++;
	
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);

	switch(p_geometry->type) {

		case Geometry::GEOMETRY_SURFACE: {

			Surface *s = (Surface*)p_geometry;
			
			bool has_tex = (s->stride > 24);
// 			bool has_tex = s->array[VS::ARRAY_TEX_UV].bind;
			
			print("stride:%d primitive:%d\n", s->stride, (int)gl_primitive[s->primitive]);

			AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=vertex
			AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3); // v1=normal
			if (has_tex)
				AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v2=tex_uv
			else
			{
// 				AttrInfo_AddFixed(attrInfo, 2);
// 				C3D_FixedAttribSet(2, 1.f, 1.f, 1.f, 1.f);
			}
			
			BufInfo_Add(bufInfo, s->array_local, s->stride, has_tex ? 3 : 2, has_tex ? 0x210 : 0x10);

			_rinfo.vertex_count+=s->array_len;

			if (s->index_array_len>0) {

				if (s->index_array_local)
				{
					print("index len: %u\n", s->array[VS::ARRAY_INDEX].size);

					//print_line("LOCAL F: "+itos(s->format)+" C: "+itos(s->index_array_len)+" VC: "+itos(s->array_len));
// 					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
// 					glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len>(1<<16))?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT, s->index_array_local);
					
					C3D_DrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array[VS::ARRAY_INDEX].size == 2) ? C3D_UNSIGNED_SHORT : C3D_UNSIGNED_BYTE, s->index_array_local);

				}

			} else {

// 				glDrawArrays(gl_primitive[s->primitive],0,s->array_len);
				C3D_DrawArrays(gl_primitive[s->primitive], 0, s->array_len);

			};

			_rinfo.draw_calls++;
		} break;

		case Geometry::GEOMETRY_MULTISURFACE: {

// 			material_shader.bind_uniforms();
			Surface *s = static_cast<const MultiMeshSurface*>(p_geometry)->surface;
			const MultiMesh *mm = static_cast<const MultiMesh*>(p_owner);
			int element_count=mm->elements.size();

			if (element_count==0)
				return;

			if (mm->visible>=0) {
				element_count=MIN(element_count,mm->visible);
			}

			const MultiMesh::Element *elements=&mm->elements[0];

			_rinfo.vertex_count+=s->array_len*element_count;

			_rinfo.draw_calls+=element_count;

/*
			if (use_texture_instancing) {
				//this is probably the fastest all around way if vertex texture fetch is supported

				float twd=(1.0/mm->tw)*4.0;
				float thd=1.0/mm->th;
				float parm[3]={0.0,01.0,(1.0f/mm->tw)};
				glActiveTexture(GL_TEXTURE0+max_texture_units-2);
				glDisableVertexAttribArray(6);
				glBindTexture(GL_TEXTURE_2D,mm->tex_id);
				material_shader.set_uniform(MaterialShaderGLES2::INSTANCE_MATRICES,GL_TEXTURE0+max_texture_units-2);

				if (s->index_array_len>0) {


					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s->index_id);
					for(int i=0;i<element_count;i++) {
						parm[0]=(i%(mm->tw>>2))*twd;
						parm[1]=(i/(mm->tw>>2))*thd;
						glVertexAttrib3fv(6,parm);
						glDrawElements(gl_primitive[s->primitive],s->index_array_len, (s->array_len>(1<<16))?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT,0);

					}


				} else {

					for(int i=0;i<element_count;i++) {
						//parm[0]=(i%(mm->tw>>2))*twd;
						//parm[1]=(i/(mm->tw>>2))*thd;
						glVertexAttrib3fv(6,parm);
						glDrawArrays(gl_primitive[s->primitive],0,s->array_len);
					}
				 };

			} else if (use_attribute_instancing) {
				//if not, using atributes instead of uniforms can be really fast in forward rendering architectures
				if (s->index_array_len>0) {


					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s->index_id);
					for(int i=0;i<element_count;i++) {
						glVertexAttrib4fv(8,&elements[i].matrix[0]);
						glVertexAttrib4fv(9,&elements[i].matrix[4]);
						glVertexAttrib4fv(10,&elements[i].matrix[8]);
						glVertexAttrib4fv(11,&elements[i].matrix[12]);
						glDrawElements(gl_primitive[s->primitive],s->index_array_len, (s->array_len>(1<<16))?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT,0);
					}


				} else {

					for(int i=0;i<element_count;i++) {
						glVertexAttrib4fv(8,&elements[i].matrix[0]);
						glVertexAttrib4fv(9,&elements[i].matrix[4]);
						glVertexAttrib4fv(10,&elements[i].matrix[8]);
						glVertexAttrib4fv(11,&elements[i].matrix[12]);
						glDrawArrays(gl_primitive[s->primitive],0,s->array_len);
					}
				 };


			} else {

				//nothing to do, slow path (hope no hardware has to use it... but you never know)

				if (s->index_array_len>0) {

					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s->index_id);
					for(int i=0;i<element_count;i++) {

						glUniformMatrix4fv(material_shader.get_uniform_location(MaterialShaderGLES2::INSTANCE_TRANSFORM), 1, false, elements[i].matrix);
						glDrawElements(gl_primitive[s->primitive],s->index_array_len, (s->array_len>(1<<16))?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT,0);
					}


				} else {

					for(int i=0;i<element_count;i++) {
						glUniformMatrix4fv(material_shader.get_uniform_location(MaterialShaderGLES2::INSTANCE_TRANSFORM), 1, false, elements[i].matrix);
						glDrawArrays(gl_primitive[s->primitive],0,s->array_len);
					}
				 };
			}
*/
		 } break;
/*
		case Geometry::GEOMETRY_IMMEDIATE: {

			bool restore_tex=false;
			const Immediate *im = static_cast<const Immediate*>( p_geometry );
			if (im->building) {
				return;
			}

			glBindBuffer(GL_ARRAY_BUFFER, 0);

			for(const List<Immediate::Chunk>::Element *E=im->chunks.front();E;E=E->next()) {

				const Immediate::Chunk &c=E->get();
				if (c.vertices.empty()) {
					continue;
				}
				for(int i=0;i<c.vertices.size();i++)

				if (c.texture.is_valid() && texture_owner.owns(c.texture)) {

					const Texture *t = texture_owner.get(c.texture);
					glActiveTexture(GL_TEXTURE0+tc0_idx);
					glBindTexture(t->target,t->tex_id);
					restore_tex=true;


				} else if (restore_tex) {

					glActiveTexture(GL_TEXTURE0+tc0_idx);
					glBindTexture(GL_TEXTURE_2D,tc0_id_cache);
					restore_tex=false;
				}

				if (!c.normals.empty()) {

					glEnableVertexAttribArray(VS::ARRAY_NORMAL);
					glVertexAttribPointer(VS::ARRAY_NORMAL, 3, GL_FLOAT, false,sizeof(Vector3),c.normals.ptr());

				} else {

					glDisableVertexAttribArray(VS::ARRAY_NORMAL);
				}

				if (!c.tangents.empty()) {

					glEnableVertexAttribArray(VS::ARRAY_TANGENT);
					glVertexAttribPointer(VS::ARRAY_TANGENT, 4, GL_FLOAT, false,sizeof(Plane),c.tangents.ptr());

				} else {

					glDisableVertexAttribArray(VS::ARRAY_TANGENT);
				}

				if (!c.colors.empty()) {

					glEnableVertexAttribArray(VS::ARRAY_COLOR);
					glVertexAttribPointer(VS::ARRAY_COLOR, 4, GL_FLOAT, false,sizeof(Color),c.colors.ptr());

				} else {

					glDisableVertexAttribArray(VS::ARRAY_COLOR);
					_set_color_attrib(Color(1, 1, 1,1));
				}


				if (!c.uvs.empty()) {

					glEnableVertexAttribArray(VS::ARRAY_TEX_UV);
					glVertexAttribPointer(VS::ARRAY_TEX_UV, 2, GL_FLOAT, false,sizeof(Vector2),c.uvs.ptr());

				} else {

					glDisableVertexAttribArray(VS::ARRAY_TEX_UV);
				}

				if (!c.uvs2.empty()) {

					glEnableVertexAttribArray(VS::ARRAY_TEX_UV2);
					glVertexAttribPointer(VS::ARRAY_TEX_UV2, 2, GL_FLOAT, false,sizeof(Vector2),c.uvs2.ptr());

				} else {

					glDisableVertexAttribArray(VS::ARRAY_TEX_UV2);
				}


				glEnableVertexAttribArray(VS::ARRAY_VERTEX);
				glVertexAttribPointer(VS::ARRAY_VERTEX, 3, GL_FLOAT, false,sizeof(Vector3),c.vertices.ptr());
				glDrawArrays(gl_primitive[c.primitive],0,c.vertices.size());


			}


			if (restore_tex) {

				glActiveTexture(GL_TEXTURE0+tc0_idx);
				glBindTexture(GL_TEXTURE_2D,tc0_id_cache);
				restore_tex=false;
			}


		} break;
`*/
		case Geometry::GEOMETRY_PARTICLES: {

			//print_line("particulinas");
			const Particles *particles = static_cast<const Particles*>( p_geometry );
			ERR_FAIL_COND(!p_owner);
			ParticlesInstance *particles_instance = (ParticlesInstance*)p_owner;

			ParticleSystemProcessSW &pp = particles_instance->particles_process;
			float td = time_delta; //MIN(time_delta,1.0/10.0);
			pp.process(&particles->data,particles_instance->transform,td);
			ERR_EXPLAIN("A parameter in the particle system is not correct.");
			ERR_FAIL_COND(!pp.valid);


			Transform camera;
// 			if (shadow)
// 				camera=shadow->transform;
// 			else
				camera=camera_transform;

			particle_draw_info.prepare(&particles->data,&pp,particles_instance->transform,camera);
			_rinfo.draw_calls+=particles->data.amount;


			_rinfo.vertex_count+=4*particles->data.amount;

			{
				static const Vector3 points[4]={
					Vector3(-1.0,1.0,0),
					Vector3(1.0,1.0,0),
					Vector3(1.0,-1.0,0),
					Vector3(-1.0,-1.0,0)
				};
				static const Vector3 uvs[4]={
					Vector3(0.0,0.0,0.0),
					Vector3(1.0,0.0,0.0),
					Vector3(1.0,1.0,0.0),
					Vector3(0,1.0,0.0)
				};
				static const Vector3 normals[4]={
					Vector3(0,0,1),
					Vector3(0,0,1),
					Vector3(0,0,1),
					Vector3(0,0,1)
				};

				static const Plane tangents[4]={
					Plane(Vector3(1,0,0),0),
					Plane(Vector3(1,0,0),0),
					Plane(Vector3(1,0,0),0),
					Plane(Vector3(1,0,0),0)
				};

				for(int i=0;i<particles->data.amount;i++) {

					ParticleSystemDrawInfoSW::ParticleDrawInfo &pinfo=*particle_draw_info.draw_info_order[i];
					if (!pinfo.data->active)
						continue;

// 					material_shader.set_uniform(MaterialShaderGLES2::WORLD_TRANSFORM, pinfo.transform);
// 					_set_color_attrib(pinfo.color);
// 					_draw_primitive(4,points,normals,NULL,uvs,tangents);
				}
			}

		} break;
		 default: break;
	};
}

void RasterizerCitro3d::_draw_quad(const Rect2& p_rect)
{	
	VertexArray *varray = memnew(VertexArray(4));
	varray->vertices[0].position = Vector3( p_rect.pos.x,p_rect.pos.y, 0.5f );
	varray->vertices[1].position = Vector3( p_rect.pos.x+p_rect.size.width,p_rect.pos.y, 0.5f );
	varray->vertices[2].position = Vector3( p_rect.pos.x+p_rect.size.width,p_rect.pos.y+p_rect.size.height, 0.5f );
	varray->vertices[3].position = Vector3( p_rect.pos.x,p_rect.pos.y+p_rect.size.height, 0.5f );
	
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, varray->vertices, sizeof(Vertex), 2, 0x10);
	
	C3D_DrawArrays(GPU_TRIANGLE_FAN, 0, 4);
	
	vertexArrays.push_back(varray);
// 	C3D_Flush();

// 	_draw_gui_primitive(4,coords,0,0);
// 	_rinfo.ci_draw_commands++;
}

void RasterizerCitro3d::_draw_textured_quad(const Rect2& p_rect, const Rect2& p_src_region, const Size2& p_tex_size,bool p_h_flip, bool p_v_flip, bool p_transpose )
{
// 	print("_draw_textured_quad\n");
	VertexArray *varray = memnew(VertexArray(4));
	
	Vertex v[4] = {
		{
			Vector3( p_rect.pos.x, p_rect.pos.y, 0.5f),
			Vector2( p_src_region.pos.x/p_tex_size.width, p_src_region.pos.y/p_tex_size.height)
		},{
			Vector3( p_rect.pos.x+p_rect.size.width, p_rect.pos.y, 0.5f),
			Vector2((p_src_region.pos.x+p_src_region.size.width)/p_tex_size.width, p_src_region.pos.y/p_tex_size.height)
		},{
			Vector3( p_rect.pos.x+p_rect.size.width, p_rect.pos.y+p_rect.size.height, 0.5f),
			Vector2( (p_src_region.pos.x+p_src_region.size.width)/p_tex_size.width, (p_src_region.pos.y+p_src_region.size.height)/p_tex_size.height)
		},{
			Vector3( p_rect.pos.x,p_rect.pos.y+p_rect.size.height, 0.5f),
			Vector2( p_src_region.pos.x/p_tex_size.width, (p_src_region.pos.y+p_src_region.size.height)/p_tex_size.height)
		},
	};
	
	memcpy(varray->vertices, v, sizeof(v));
	
	if (p_transpose) {
		SWAP( varray->vertices[1].texcoord, varray->vertices[3].texcoord );
	}
	if (p_h_flip) {
		SWAP( varray->vertices[0].texcoord, varray->vertices[1].texcoord );
		SWAP( varray->vertices[2].texcoord, varray->vertices[3].texcoord );
	}
	if (p_v_flip) {
		SWAP( varray->vertices[1].texcoord, varray->vertices[2].texcoord );
		SWAP( varray->vertices[0].texcoord, varray->vertices[3].texcoord );
	}
	
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, varray->vertices, sizeof(Vertex), 2, 0x10);
	
	C3D_DrawArrays(GPU_TRIANGLE_FAN, 0, 4);

	vertexArrays.push_back(varray);
	
// 	_draw_gui_primitive(4,coords,0,texcoords);
// 	_rinfo.ci_draw_commands++;
}

static const C3D_Material material =
	{
		{ 0.2f, 0.2f, 0.2f }, //ambient
		{ 0.4f, 0.4f, 0.4f }, //diffuse
		{ 0.8f, 0.8f, 0.8f }, //specular0
		{ 0.0f, 0.0f, 0.0f }, //specular1
		{ 0.0f, 0.0f, 0.0f }, //emission
	};

int RasterizerCitro3d::RenderList::max_elements;

void RasterizerCitro3d::init()
{	
	print("citro init ");
	C3D_Init(0x100000);
	
	draw_next_frame = false;

	canvas_shader = memnew(Shader);
	canvas_shader->set_data(shader_builtin_2d, sizeof(shader_builtin_2d));
	
	scene_shader = memnew(Shader);
	scene_shader->set_data(shader_builtin_3d, sizeof(shader_builtin_3d));

	// Setup default render target (screen framebuffer)
	base_framebuffer = memnew(RenderTarget);	
	Texture *texture = memnew(Texture);

	base_framebuffer->target = C3D_RenderTargetCreate(480, 800, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(base_framebuffer->target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetClear(base_framebuffer->target, C3D_CLEAR_ALL, 0x0, 0);
	
	base_framebuffer->texture_ptr = texture;
	base_framebuffer->texture = texture_owner.make_rid( texture );
	
	// Frag lighting
	C3D_LightEnvInit(&lightEnv);
	C3D_LightEnvBind(&lightEnv);
	C3D_LightEnvMaterial(&lightEnv, &material);
	
	LightLut_Phong(&lut_Phong, 30);
	C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN, false, &lut_Phong);
	
	C3D_FVec lightVec = { { 1.0, -1.5, 0.0, 0.0 } };

// 	C3D_LightInit(&light, &lightEnv);
// 	C3D_LightColor(&light, 1.0, 1.0, 1.0);
// 	C3D_LightPosition(&light, &lightVec);
	
// 	C3D_LightSpotEnable(&light, true);
// 	C3D_LightSpotDir(&light, 1.0, 1.0, 1.0);
	
	C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);
// 	C3D_CullFace(GPU_CULL_FRONT_CCW);
	C3D_CullFace(GPU_CULL_NONE);
	
	RenderList::max_elements=GLOBAL_DEF("rasterizer/max_render_elements",(int)RenderList::DEFAULT_MAX_ELEMENTS);
	if (RenderList::max_elements>64000)
		RenderList::max_elements=64000;
	if (RenderList::max_elements<1024)
		RenderList::max_elements=1024;
	
	opaque_render_list.init();
	alpha_render_list.init();
}

void RasterizerCitro3d::finish()
{
	print("citro finish\n");
	
	memdelete(canvas_shader);
	memdelete(scene_shader);
	memdelete(base_framebuffer->texture_ptr);
	memdelete(base_framebuffer);
	
	C3D_Fini();
}

int RasterizerCitro3d::get_render_info(VS::RenderInfo p_info) {

	switch(p_info) {

		case VS::INFO_OBJECTS_IN_FRAME: {

			return _rinfo.object_count;
		} break;
		case VS::INFO_VERTICES_IN_FRAME: {

			return _rinfo.vertex_count;
		} break;
		case VS::INFO_MATERIAL_CHANGES_IN_FRAME: {

			return _rinfo.mat_change_count;
		} break;
		case VS::INFO_SHADER_CHANGES_IN_FRAME: {

			return _rinfo.shader_change_count;
		} break;
		case VS::INFO_DRAW_CALLS_IN_FRAME: {

			return _rinfo.draw_calls;
		} break;
		case VS::INFO_SURFACE_CHANGES_IN_FRAME: {

			return _rinfo.surface_count;
		} break;
		case VS::INFO_USAGE_VIDEO_MEM_TOTAL: {

			return 0;
		} break;
		case VS::INFO_VIDEO_MEM_USED: {

			return get_render_info(VS::INFO_TEXTURE_MEM_USED)+get_render_info(VS::INFO_VERTEX_MEM_USED);
		} break;
		case VS::INFO_TEXTURE_MEM_USED: {

			return _rinfo.texture_mem;
		} break;
		case VS::INFO_VERTEX_MEM_USED: {

			return 0;
		} break;
	}

	return 0;
}

bool RasterizerCitro3d::needs_to_draw_next_frame() const
{
	return false;
	return draw_next_frame;
}


bool RasterizerCitro3d::has_feature(VS::Features p_feature) const
{
	switch (p_feature) {
		default:
		case VS::FEATURE_MULTITHREADED:     return false;
		case VS::FEATURE_SHADERS:           return true;
		case VS::FEATURE_NEEDS_RELOAD_HOOK: return false;
	}
}

void RasterizerCitro3d::restore_framebuffer()
{
	print("restore_framebuffer\n");
	current_rt = NULL;
	C3D_RenderBufBind(&base_framebuffer->target->renderBuf);
}

RasterizerCitro3d::RasterizerCitro3d()
: current_rt (NULL)
{
	
};

RasterizerCitro3d::~RasterizerCitro3d()
{

};

#endif
