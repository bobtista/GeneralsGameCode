/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/sortingrenderer.cpp                    $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Changes to max texture stage caps																*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <stdlib.h>

#include "sortingrenderer.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dllist.h"
#include "FixedFunctionState.h"
#include "RenderBufferTypes.h"
#include "RenderBackend.h"
#include "IRenderBackend.h"
#include "vertmaterial.h"
#include "texture.h"
#include "statistics.h"
#include "WW3D2/BgfxRenderProfile.h"
#include "GgcRuntimeFlags.h"
#include "WW3D2/ww3d.h"
#include <WWDebug/wwprofile.h>
#include <algorithm>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>


bool SortingRendererClass::_EnableTriangleDraw=true;
static unsigned DEFAULT_SORTING_POLY_COUNT = 16384;	// (count * 3) must be less than 65536
static unsigned DEFAULT_SORTING_VERTEX_COUNT = 32768;	// count must be less than 65536

void SortingRendererClass::SetMinVertexBufferSize( unsigned val )
{
	DEFAULT_SORTING_VERTEX_COUNT = val;
	DEFAULT_SORTING_POLY_COUNT = val/2;	//typically have 2:1 vertex:triangle ratio.
}

struct ShortVectorIStruct
{
	unsigned short i;
	unsigned short j;
	unsigned short k;
};

struct TempIndexStruct
{
	ShortVectorIStruct tri;
	unsigned short idx;
	float z;
};

enum BgfxSortedPacketDiagSource : unsigned char
{
	BGFX_SORTED_PACKET_SOURCE_OTHER = 0,
	BGFX_SORTED_PACKET_SOURCE_POINT_GROUP,
	BGFX_SORTED_PACKET_SOURCE_STREAK
};

enum BgfxSortedPacketDiagFallback : unsigned char
{
	BGFX_SORTED_PACKET_FALLBACK_NONE = 0,
	BGFX_SORTED_PACKET_FALLBACK_DEPTH_WRITE,
	BGFX_SORTED_PACKET_FALLBACK_ALPHA_TEST,
	BGFX_SORTED_PACKET_FALLBACK_NON_ADDITIVE_BLEND,
	BGFX_SORTED_PACKET_FALLBACK_UNKNOWN
};

enum BgfxSortedPacketDiagBlendClass : unsigned
{
	BGFX_SORTED_PACKET_BLEND_ALPHA = 0,
	BGFX_SORTED_PACKET_BLEND_MULTIPLY,
	BGFX_SORTED_PACKET_BLEND_SCREEN,
	BGFX_SORTED_PACKET_BLEND_SRC_ALPHA_ONE,
	BGFX_SORTED_PACKET_BLEND_OTHER,
	BGFX_SORTED_PACKET_BLEND_COUNT
};

static bool Sorted_Packet_Collector_Diag_Enabled()
{
	static const bool enabled = GgcFlags::Enabled(GgcFlag_BgfxSortedPacketCollectorDiag);
	return enabled;
}

static unsigned Sorted_Packet_Collector_Diag_Max_Flushes()
{
	static const unsigned maxFlushes = []() -> unsigned {
		const char* env = GgcFlags::StringValue(GgcFlag_BgfxSortedPacketCollectorDiagLimit);
		if (env == nullptr)
		{
			return 512;
		}
		const int parsed = std::atoi(env);
		return parsed > 0 ? static_cast<unsigned>(parsed) : 512;
	}();
	return maxFlushes;
}

static inline bool Is_Additive_Sorted_Packet_Candidate(const RenderStateStruct& rs)
{
	return rs.shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE
		&& rs.shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE
		&& rs.shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE
		&& rs.shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE;
}

static BgfxSortedPacketDiagSource Classify_Sorted_Packet_Diag_Source(const RenderStateStruct& rs)
{
	if ((rs.sorted_draw_flags & RB_SORTED_DRAW_POINT_GROUP) != 0)
	{
		return BGFX_SORTED_PACKET_SOURCE_POINT_GROUP;
	}
	if ((rs.sorted_draw_flags & RB_SORTED_DRAW_STREAK) != 0)
	{
		return BGFX_SORTED_PACKET_SOURCE_STREAK;
	}
	return BGFX_SORTED_PACKET_SOURCE_OTHER;
}

static BgfxSortedPacketDiagFallback Classify_Sorted_Packet_Diag_Fallback(const RenderStateStruct& rs)
{
	if (Is_Additive_Sorted_Packet_Candidate(rs))
	{
		return BGFX_SORTED_PACKET_FALLBACK_NONE;
	}
	if (rs.shader.Get_Depth_Mask() != ShaderClass::DEPTH_WRITE_DISABLE)
	{
		return BGFX_SORTED_PACKET_FALLBACK_DEPTH_WRITE;
	}
	if (rs.shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE)
	{
		return BGFX_SORTED_PACKET_FALLBACK_ALPHA_TEST;
	}
	if (rs.shader.Get_Src_Blend_Func() != ShaderClass::SRCBLEND_ONE
		|| rs.shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ONE)
	{
		return BGFX_SORTED_PACKET_FALLBACK_NON_ADDITIVE_BLEND;
	}
	return BGFX_SORTED_PACKET_FALLBACK_UNKNOWN;
}

static BgfxSortedPacketDiagBlendClass Classify_Sorted_Packet_Diag_Blend(const RenderStateStruct& rs)
{
	const ShaderClass::SrcBlendFuncType src = rs.shader.Get_Src_Blend_Func();
	const ShaderClass::DstBlendFuncType dst = rs.shader.Get_Dst_Blend_Func();
	if (src == ShaderClass::SRCBLEND_SRC_ALPHA
		&& dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA)
	{
		return BGFX_SORTED_PACKET_BLEND_ALPHA;
	}
	if (src == ShaderClass::SRCBLEND_ZERO
		&& dst == ShaderClass::DSTBLEND_SRC_COLOR)
	{
		return BGFX_SORTED_PACKET_BLEND_MULTIPLY;
	}
	if (src == ShaderClass::SRCBLEND_ONE
		&& dst == ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR)
	{
		return BGFX_SORTED_PACKET_BLEND_SCREEN;
	}
	if (src == ShaderClass::SRCBLEND_SRC_ALPHA
		&& dst == ShaderClass::DSTBLEND_ONE)
	{
		return BGFX_SORTED_PACKET_BLEND_SRC_ALPHA_ONE;
	}
	return BGFX_SORTED_PACKET_BLEND_OTHER;
}

static Matrix4x4 Multiply_Sorted_Matrix(const Matrix4x4& lhs, const Matrix4x4& rhs)
{
	Matrix4x4 result;
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			result[row][col] =
				lhs[row][0] * rhs[0][col] +
				lhs[row][1] * rhs[1][col] +
				lhs[row][2] * rhs[2][col] +
				lhs[row][3] * rhs[3][col];
		}
	}
	return result;
}

static Matrix4x4 Get_Sorted_World_View_Matrix(const RenderStateStruct& state)
{
	static_assert(sizeof(state.world) == sizeof(Matrix4x4), "sorted matrix snapshot must match Matrix4x4 for reinterpret_cast");
	return Multiply_Sorted_Matrix(
		reinterpret_cast<const Matrix4x4&>(state.world),
		reinterpret_cast<const Matrix4x4&>(state.view));
}

static Vector3 Transform_Sorted_Point(const Vector3& point, const Matrix4x4& matrix)
{
	return Vector3(
		point.X * matrix[0][0] + point.Y * matrix[1][0] + point.Z * matrix[2][0] + matrix[3][0],
		point.X * matrix[0][1] + point.Y * matrix[1][1] + point.Z * matrix[2][1] + matrix[3][1],
		point.X * matrix[0][2] + point.Y * matrix[1][2] + point.Z * matrix[2][2] + matrix[3][2]);
}

static RenderBackendSortedBatchState Make_Render_Backend_Sorted_State(const RenderStateStruct& render_state);
static inline bool Is_Coalescable_Additive(const RenderStateStruct& rs);

bool operator <(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z < r.z; }
bool operator <=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z <= r.z; }
bool operator >(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z > r.z; }
bool operator >=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z >= r.z; }
bool operator ==(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z == r.z; }
// ----------------------------------------------------------------------------
static
void InsertionSort(TempIndexStruct *begin, TempIndexStruct *end)
{
	for (TempIndexStruct *iter = begin + 1; iter < end; ++iter) {
		TempIndexStruct val = iter[0];
		TempIndexStruct *insert = iter;
		while (insert != begin && insert[-1] > val) {
			insert[0] = insert[-1];
			insert -= 1;
		}
		insert[0] = val;
	}
}

// ----------------------------------------------------------------------------
static
void Sort(TempIndexStruct *begin, TempIndexStruct *end)
{
	const int diff = end - begin;
	if (diff <= 16) {
		// Insertion sort has less overhead for small arrays
		InsertionSort(begin, end);
	} else {
		// Choose the median of begin, mid, and (end - 1) as the partitioning element.
		// Rearrange so that *(begin + 1) <= *begin <= *(end - 1).  These will be guard
		// elements.
		TempIndexStruct *mid = begin + diff/2;
		std::swap(mid[0], begin[1]);
		if (begin[1] > end[-1]) {
			std::swap(begin[1], end[-1]);
		}
		if (begin[0] > end[-1]) {
			std::swap(begin[0], end[-1]);
		}
		if (begin[1] > begin[0]) {
			std::swap(begin[1], begin[0]);
		}

		// *begin is now the partitioning element
		TempIndexStruct *begin1 = begin + 1;	// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *end1 = end - 1;			// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *left = begin + 1;
		TempIndexStruct *right = end - 1;
		for (;;) {
#if 0		// TODO: Temp fix until I find out who is passing me NaN.
			do ++left; while (left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right[0] > begin[0]);	// Scan down to find element <= than partition
#else
			do ++left; while (left < end1 && left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right > begin1 && right[0] > begin[0]);	// Scan down to find element <= than partition
#endif
			if (right < left) break;									// Pointers crossed.  Partitioning completed.
			std::swap(left[0], right[0]);							// Exchange elements.
		}
		std::swap(begin[0], right[0]);							// Insert partition element

		// Sort the smaller subarray first then the larger
		if (right - begin > end - (right + 1)) {
			Sort(right + 1, end);
			Sort(begin, right);
		} else {
			Sort(begin, right);
			Sort(right + 1, end);
		}
	}
}

// ----------------------------------------------------------------------------

class SortingNodeStruct
{
	W3DMPO_CODE(SortingNodeStruct)

public:
	RenderStateStruct sorting_state;

	Vector3 transformed_center;
	unsigned short start_index;			// First index used in the ib
	unsigned short polygon_count;			// Polygon count to process (3 indices = one polygon)
	unsigned short min_vertex_index;		// First index used in the vb
	unsigned short vertex_count;			// Number of vertices used in vb
	RenderBackendSortedBatchState render_backend_state;
	unsigned char packet_diag_source;
	unsigned char packet_diag_fallback;
};

typedef std::list<SortingNodeStruct*> SortingNodeStructList;
static SortingNodeStructList sorted_list;
static SortingNodeStructList unsorted_list;
static SortingNodeStructList clean_list;
static unsigned total_sorting_vertices;

static SortingNodeStruct* Get_Sorting_Struct()
{
	if (!clean_list.empty()) {
		SortingNodeStruct* state = clean_list.front();
		clean_list.pop_front();
		return state;
	}
	return W3DNEW SortingNodeStruct();
}

// ----------------------------------------------------------------------------
//
// Temporary arrays for the sorting system
//
// ----------------------------------------------------------------------------

static TempIndexStruct* temp_index_array;
static unsigned temp_index_array_count;

static TempIndexStruct* Get_Temp_Index_Array(unsigned count)
{
	if (count < DEFAULT_SORTING_POLY_COUNT)
		count = DEFAULT_SORTING_POLY_COUNT;
	if (count>temp_index_array_count) {
		delete[] temp_index_array;
		temp_index_array=W3DNEWARRAY TempIndexStruct[count];
		temp_index_array_count=count;
	}
	return temp_index_array;
}

// ----------------------------------------------------------------------------
//
// Insert triangles to the sorting system.
//
// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_Triangles(
	const SphereClass& bounding_sphere,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (!WW3D::Is_Sorting_Enabled()) {
		g_renderBackend->Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
		return;
	}

	SNAPSHOT_SAY(("SortingRenderer::Insert(start_i: %d, polygons : %d, min_vi: %d, vertex_count: %d)",
		start_index,polygon_count,min_vertex_index,vertex_count));


	DX8_RECORD_SORTING_RENDER(polygon_count,vertex_count);

	SortingNodeStruct* state=Get_Sorting_Struct();

	g_renderBackend->Capture_Legacy_Render_State_For_Sorted_Draw(state->sorting_state);

 	WWASSERT(
		((state->sorting_state.index_buffer_type==BUFFER_TYPE_SORTING || state->sorting_state.index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) &&
		(state->sorting_state.vertex_buffer_types[0]==BUFFER_TYPE_SORTING || state->sorting_state.vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING)));


	state->start_index=start_index;
	state->polygon_count=polygon_count;
	state->min_vertex_index=min_vertex_index;
	state->vertex_count=vertex_count;
	if (g_renderBackend->Has_Shader_Pipeline() && Sorted_Packet_Collector_Diag_Enabled())
	{
		state->packet_diag_source = static_cast<unsigned char>(Classify_Sorted_Packet_Diag_Source(state->sorting_state));
		state->packet_diag_fallback = static_cast<unsigned char>(Classify_Sorted_Packet_Diag_Fallback(state->sorting_state));
	}
	else
	{
		state->packet_diag_source = BGFX_SORTED_PACKET_SOURCE_OTHER;
		state->packet_diag_fallback = BGFX_SORTED_PACKET_FALLBACK_UNKNOWN;
	}
	state->render_backend_state = Make_Render_Backend_Sorted_State(state->sorting_state);

	if (bounding_sphere.Is_Valid())
	{
		const Matrix4x4 mtx = Get_Sorted_World_View_Matrix(state->sorting_state);
		state->transformed_center = Transform_Sorted_Point(bounding_sphere.Center, mtx);

		Insert_To_Sorted_List(state);
	}
	else
	{
		// TheSuperHackers @perf stephanmeesters 04/07/2026 Nodes without bounding information do not require sorting.
		state->transformed_center = Vector3(0.0f, 0.0f, 0.0f);
		unsorted_list.push_back(state);
	}

#ifdef WWDEBUG
	SortingVertexBufferClass* vertex_buffer=static_cast<SortingVertexBufferClass*>(state->sorting_state.vertex_buffers[0]);
	WWASSERT(vertex_buffer);
	WWASSERT(state->vertex_count<=vertex_buffer->Get_Vertex_Count());

	unsigned short* indices=nullptr;
	SortingIndexBufferClass* index_buffer=static_cast<SortingIndexBufferClass*>(state->sorting_state.index_buffer);
	WWASSERT(index_buffer);
	indices=index_buffer->index_buffer;
	WWASSERT(indices);
	indices+=state->start_index;
	indices+=state->sorting_state.iba_offset;

	for (int i=0;i<state->polygon_count;++i) {
		unsigned short idx1=indices[i*3]-state->min_vertex_index;
		unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
		unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
		WWASSERT(idx1<state->vertex_count);
		WWASSERT(idx2<state->vertex_count);
		WWASSERT(idx3<state->vertex_count);
	}
#endif // WWDEBUG
}

// ----------------------------------------------------------------------------
//
// Insert triangles to the sorting system, with no bounding information.
//
// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Insert_Triangles(SphereClass(),start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
// Flush all sorting polygons.
//
// ----------------------------------------------------------------------------

void Release_Refs(SortingNodeStruct* state)
{
	int i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(state->sorting_state.vertex_buffers[i]);
	}
	REF_PTR_RELEASE(state->sorting_state.index_buffer);
	REF_PTR_RELEASE(state->sorting_state.material);
	for (i=0;i<RB_MAX_TEXTURE_STAGES;++i)
	{
		REF_PTR_RELEASE(state->sorting_state.Textures[i]);
	}
}

static unsigned overlapping_node_count;
static unsigned overlapping_polygon_count;
static unsigned overlapping_vertex_count;
static const unsigned MAX_OVERLAPPING_NODES=4096;
static SortingNodeStruct* overlapping_nodes[MAX_OVERLAPPING_NODES];

// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_To_Sorted_List(SortingNodeStruct *state)
{
	/// @todo lorenzen sez use a bucket sort here... and stop copying so much data so many times

	for (SortingNodeStructList::iterator node = sorted_list.begin(); node != sorted_list.end(); ++node)
	{
		if (state->transformed_center.Z > (*node)->transformed_center.Z) {
			sorted_list.insert(node, state);
			return;
		}
	}

	sorted_list.push_back(state);
}

// ----------------------------------------------------------------------------

void SortingRendererClass::Insert_To_Sorting_Pool(SortingNodeStruct* state)
{
	if (overlapping_node_count>=MAX_OVERLAPPING_NODES) {
		Release_Refs(state);
		delete state;
		WWASSERT(0);
		return;
	}

	overlapping_nodes[overlapping_node_count]=state;
	overlapping_vertex_count+=state->vertex_count;
	overlapping_polygon_count+=state->polygon_count;
	overlapping_node_count++;
}

// ----------------------------------------------------------------------------
//static unsigned prevLight = 0xffffffff;

static RenderBackendLight Make_Render_Backend_Light(const RenderStateStruct & render_state, int index)
{
	const auto & light = render_state.Lights[index];
	RenderBackendLight rb_light;
	rb_light.type = light.Type;
	rb_light.position[0] = light.Position.x;
	rb_light.position[1] = light.Position.y;
	rb_light.position[2] = light.Position.z;
	rb_light.direction[0] = light.Direction.x;
	rb_light.direction[1] = light.Direction.y;
	rb_light.direction[2] = light.Direction.z;
	rb_light.diffuse[0] = light.Diffuse.r;
	rb_light.diffuse[1] = light.Diffuse.g;
	rb_light.diffuse[2] = light.Diffuse.b;
	rb_light.ambient[0] = light.Ambient.r;
	rb_light.ambient[1] = light.Ambient.g;
	rb_light.ambient[2] = light.Ambient.b;
	rb_light.specular[0] = light.Specular.r;
	rb_light.specular[1] = light.Specular.g;
	rb_light.specular[2] = light.Specular.b;
	rb_light.range = light.Range;
	rb_light.falloff = light.Falloff;
	rb_light.attenuation[0] = light.Attenuation0;
	rb_light.attenuation[1] = light.Attenuation1;
	rb_light.attenuation[2] = light.Attenuation2;
	rb_light.theta = light.Theta;
	rb_light.phi = light.Phi;
	return rb_light;
}

static RenderBackendSortedMaterialSnapshot Make_Render_Backend_Sorted_Material_Snapshot(const RenderStateStruct& render_state)
{
	RenderBackendSortedMaterialSnapshot snapshot;
	snapshot.valid = true;
	const VertexMaterialClass* material = render_state.material;
	if (material == nullptr)
	{
		return snapshot;
	}

	Vector3 diffuse(1.0f, 1.0f, 1.0f);
	Vector3 ambient(1.0f, 1.0f, 1.0f);
	const VertexMaterialClass::ColorSourceType diffuse_source =
		const_cast<VertexMaterialClass*>(material)->Get_Diffuse_Color_Source();
	const VertexMaterialClass::ColorSourceType ambient_source =
		const_cast<VertexMaterialClass*>(material)->Get_Ambient_Color_Source();
	const VertexMaterialClass::ColorSourceType emissive_source =
		const_cast<VertexMaterialClass*>(material)->Get_Emissive_Color_Source();
	if (diffuse_source == VertexMaterialClass::MATERIAL)
	{
		material->Get_Diffuse(&diffuse);
	}
	if (ambient_source == VertexMaterialClass::MATERIAL)
	{
		material->Get_Ambient(&ambient);
	}
	snapshot.diffuse[0] = diffuse.X;
	snapshot.diffuse[1] = diffuse.Y;
	snapshot.diffuse[2] = diffuse.Z;
	snapshot.diffuse[3] = material->Get_Opacity();
	snapshot.ambient[0] = ambient.X;
	snapshot.ambient[1] = ambient.Y;
	snapshot.ambient[2] = ambient.Z;
	snapshot.ambient[3] = 1.0f;
	snapshot.vertex_color_flags[1] = (diffuse_source == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
	snapshot.vertex_color_flags[2] = (ambient_source == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
	snapshot.vertex_color_flags[3] = (emissive_source == VertexMaterialClass::COLOR1) ? 1.0f : 0.0f;
	snapshot.lighting_enabled[0] =
		(material->Get_Lighting() && !WW3D::Is_Coloring_Enabled()) ? 1.0f : 0.0f;

	Vector3 emissive(0.0f, 0.0f, 0.0f);
	material->Get_Emissive(&emissive);
	snapshot.emissive[0] = emissive.X;
	snapshot.emissive[1] = emissive.Y;
	snapshot.emissive[2] = emissive.Z;
	snapshot.emissive[3] = 0.0f;

	Vector3 specular(0.0f, 0.0f, 0.0f);
	material->Get_Specular(&specular);
	snapshot.specular[0] = specular.X;
	snapshot.specular[1] = specular.Y;
	snapshot.specular[2] = specular.Z;
	snapshot.specular[3] = material->Get_Shininess();
	return snapshot;
}

static RenderBackendSortedBatchState Make_Render_Backend_Sorted_State(const RenderStateStruct& render_state)
{
	static_assert(sizeof(render_state.world) == sizeof(Matrix4x4), "sorted matrix snapshot must match Matrix4x4 for reinterpret_cast");
	RenderBackendSortedBatchState rb_state;
	rb_state.shader = &render_state.shader;
	rb_state.material = render_state.material;
	for (unsigned i = 0; i < RB_MAX_TEXTURE_STAGES; ++i)
	{
		rb_state.textures[i] = render_state.Textures[i];
	}
	rb_state.world = &reinterpret_cast<const Matrix4x4&>(render_state.world);
	rb_state.view = &reinterpret_cast<const Matrix4x4&>(render_state.view);
	const bool use_lights = (render_state.material != nullptr && render_state.material->Get_Lighting());
	for (int i = 0; i < 4; ++i)
	{
		rb_state.lights.lights[i] = Make_Render_Backend_Light(render_state, i);
		rb_state.lights.enabled[i] = use_lights && render_state.LightEnable[i];
	}
	rb_state.material_snapshot = Make_Render_Backend_Sorted_Material_Snapshot(render_state);
	rb_state.draw_flags = render_state.sorted_draw_flags;
	return rb_state;
}

static bool Sorted_Batch_State_Packet_Cache_Disabled()
{
	static const bool disabled = GgcFlags::Enabled(GgcFlag_BgfxDisableSortedBatchStatePacketCache);
	return disabled;
}

static void Apply_Render_State(SortingNodeStruct* state)
{
	if (Sorted_Batch_State_Packet_Cache_Disabled())
	{
		g_renderBackend->Apply_Sorted_Batch_State(Make_Render_Backend_Sorted_State(state->sorting_state));
		return;
	}
	g_renderBackend->Apply_Sorted_Batch_State(state->render_backend_state);
}

static bool Should_Log_Sort_Effect_Diag()
{
	static const bool enabled = GgcFlags::Enabled(GgcFlag_SortEffectDiag);
	return enabled;
}

static void Log_Sort_Effect_Diag(const char* event, unsigned start_index, unsigned polygon_count, SortingNodeStruct* state)
{
	if (!Should_Log_Sort_Effect_Diag())
	{
		return;
	}

	TextureClass* tex0 = (state != nullptr && state->sorting_state.Textures[0] != nullptr)
		? state->sorting_state.Textures[0]->As_TextureClass()
		: nullptr;
	const char* texName = tex0 != nullptr ? tex0->Get_Full_Path().str() : "(null)";
	static const bool logAll = GgcFlags::Enabled(GgcFlag_SortEffectDiagAll);
	if (!logAll
		&& strnicmp(texName, "ex", 2) != 0
		&& std::strstr(texName, "fire") == nullptr
		&& std::strstr(texName, "smoke") == nullptr
		&& std::strstr(texName, "noise") == nullptr)
	{
		return;
	}

	float worldTx = 0.0f, worldTy = 0.0f, worldTz = 0.0f;
	if (state != nullptr)
	{
		const Matrix4x4& w = reinterpret_cast<const Matrix4x4&>(state->sorting_state.world);
		worldTx = w[3][0];
		worldTy = w[3][1];
		worldTz = w[3][2];
	}
	float centerZ = state != nullptr ? state->transformed_center.Z : 0.0f;

	if (FILE* diag = std::fopen("ggc_sort_effect_diag.txt", "a"))
	{
		std::fprintf(diag,
			"%s polys=%u tex=%s shader=0x%08x worldT=(%.2f,%.2f,%.2f) centerZ=%.3f\n",
			event,
			polygon_count,
			texName,
			state != nullptr ? state->sorting_state.shader.Get_Bits() : 0,
			worldTx, worldTy, worldTz, centerZ);
		std::fclose(diag);
	}
}

// ----------------------------------------------------------------------------

static bool Render_State_Matches(const RenderStateStruct& left, const RenderStateStruct& right)
{
	if (left.shader.Get_Bits() != right.shader.Get_Bits())
	{
		return false;
	}

	if (left.sorted_draw_flags != right.sorted_draw_flags)
	{
		return false;
	}

	if (left.material != right.material)
	{
		return false;
	}

	for (int texture_index=0; texture_index<g_renderBackend->Get_Max_Texture_Stages(); ++texture_index)
	{
		if (left.Textures[texture_index] != right.Textures[texture_index])
		{
			return false;
		}
	}

	if (std::memcmp(&left.world,&right.world,sizeof(left.world)) != 0)
	{
		return false;
	}

	if (std::memcmp(&left.view,&right.view,sizeof(left.view)) != 0)
	{
		return false;
	}

	for (int light_index=0; light_index<4; ++light_index)
	{
		if (left.LightEnable[light_index] != right.LightEnable[light_index])
		{
			return false;
		}

		if (left.LightEnable[light_index] && std::memcmp(&left.Lights[light_index],&right.Lights[light_index],sizeof(left.Lights[light_index])) != 0)
		{
			return false;
		}
	}

	return true;
}

// TheSuperHackers @performance bobtista 08/07/2026 True when two sorted states
// are identical in every dimension Render_State_Matches checks EXCEPT the
// stage-0 texture. Such adjacent runs can merge into one texture-array draw
// without any reordering: the z-sorted triangle order is preserved and only
// the per-vertex layer index distinguishes the textures.
static bool Render_State_Matches_Except_Stage0_Texture(const RenderStateStruct& left, const RenderStateStruct& right)
{
	if (left.shader.Get_Bits() != right.shader.Get_Bits())
	{
		return false;
	}
	if (left.sorted_draw_flags != right.sorted_draw_flags)
	{
		return false;
	}
	if (left.material != right.material)
	{
		return false;
	}
	for (int texture_index=1; texture_index<g_renderBackend->Get_Max_Texture_Stages(); ++texture_index)
	{
		if (left.Textures[texture_index] != right.Textures[texture_index])
		{
			return false;
		}
	}
	if (std::memcmp(&left.world,&right.world,sizeof(left.world)) != 0)
	{
		return false;
	}
	if (std::memcmp(&left.view,&right.view,sizeof(left.view)) != 0)
	{
		return false;
	}
	for (int light_index=0; light_index<4; ++light_index)
	{
		if (left.LightEnable[light_index] != right.LightEnable[light_index])
		{
			return false;
		}
		if (left.LightEnable[light_index] && std::memcmp(&left.Lights[light_index],&right.Lights[light_index],sizeof(left.Lights[light_index])) != 0)
		{
			return false;
		}
	}
	return true;
}

// Per-flush page/layer of each overlapping node; -1 when the node cannot
// join a texture-array merged run. Rebuilt in Flush_Sorting_Pool.
static std::vector<int> s_sortedNodeArrayPage;
unsigned s_sortedArrayBreakMaskCounts[256] = {};
static unsigned s_sortedArrayDbgNodes = 0;
static unsigned s_sortedArrayDbgEligible = 0;
static unsigned s_sortedArrayDbgMerges = 0;
static unsigned s_sortedArrayDbgFlushes = 0;

enum BgfxSortedPacketBreakReason : unsigned
{
	BGFX_SORTED_PACKET_BREAK_SHADER = 0,
	BGFX_SORTED_PACKET_BREAK_FLAGS,
	BGFX_SORTED_PACKET_BREAK_MATERIAL,
	BGFX_SORTED_PACKET_BREAK_TEXTURE,
	BGFX_SORTED_PACKET_BREAK_WORLD,
	BGFX_SORTED_PACKET_BREAK_VIEW,
	BGFX_SORTED_PACKET_BREAK_LIGHT_ENABLE,
	BGFX_SORTED_PACKET_BREAK_LIGHT_PAYLOAD,
	BGFX_SORTED_PACKET_BREAK_COUNT
};

static unsigned Sorted_State_Break_Mask(const RenderStateStruct& left, const RenderStateStruct& right)
{
	unsigned mask = 0;
	if (left.shader.Get_Bits() != right.shader.Get_Bits())
	{
		mask |= 1u << BGFX_SORTED_PACKET_BREAK_SHADER;
	}
	if (left.sorted_draw_flags != right.sorted_draw_flags)
	{
		mask |= 1u << BGFX_SORTED_PACKET_BREAK_FLAGS;
	}
	if (left.material != right.material)
	{
		mask |= 1u << BGFX_SORTED_PACKET_BREAK_MATERIAL;
	}
	for (int texture_index = 0; texture_index < g_renderBackend->Get_Max_Texture_Stages(); ++texture_index)
	{
		if (left.Textures[texture_index] != right.Textures[texture_index])
		{
			mask |= 1u << BGFX_SORTED_PACKET_BREAK_TEXTURE;
			break;
		}
	}
	if (std::memcmp(&left.world, &right.world, sizeof(left.world)) != 0)
	{
		mask |= 1u << BGFX_SORTED_PACKET_BREAK_WORLD;
	}
	if (std::memcmp(&left.view, &right.view, sizeof(left.view)) != 0)
	{
		mask |= 1u << BGFX_SORTED_PACKET_BREAK_VIEW;
	}
	for (int light_index = 0; light_index < 4; ++light_index)
	{
		if (left.LightEnable[light_index] != right.LightEnable[light_index])
		{
			mask |= 1u << BGFX_SORTED_PACKET_BREAK_LIGHT_ENABLE;
			continue;
		}
		if (left.LightEnable[light_index]
			&& std::memcmp(&left.Lights[light_index], &right.Lights[light_index], sizeof(left.Lights[light_index])) != 0)
		{
			mask |= 1u << BGFX_SORTED_PACKET_BREAK_LIGHT_PAYLOAD;
		}
	}
	return mask;
}

static unsigned Sorted_State_Texture_Break_Mask(const RenderStateStruct& left, const RenderStateStruct& right)
{
	unsigned mask = 0;
	for (int texture_index = 0; texture_index < g_renderBackend->Get_Max_Texture_Stages(); ++texture_index)
	{
		if (left.Textures[texture_index] != right.Textures[texture_index])
		{
			mask |= 1u << texture_index;
		}
	}
	return mask;
}

static const char* Sorted_Texture_Full_Path(const RenderStateStruct& state, unsigned stage)
{
	if (stage >= RB_MAX_TEXTURE_STAGES || state.Textures[stage] == nullptr)
	{
		return nullptr;
	}
	TextureClass* texture = state.Textures[stage]->As_TextureClass();
	return texture != nullptr ? texture->Get_Full_Path().str() : nullptr;
}

// ----------------------------------------------------------------------------
// TheSuperHackers @performance bobtista 04/06/2026 Coalesce z-scattered
// additive sorted triangles that share render state into contiguous runs so
// Flush_Sorting_Pool emits one draw per state instead of one draw per
// depth-interleaved run. Additive blend (SRCBLEND_ONE/DSTBLEND_ONE) with depth
// writes disabled is order-independent for non-negative fragments even under
// the per-fragment [0,1] clamp, so reordering additive triangles within a run
// bounded by any non-additive (barrier) triangle is pixel-identical. Only the
// shader pipeline runs this; GGC_NO_SORT_COALESCE reverts to the byte-identical
// z-only path and the DX8 backend never enters it.

static bool Sort_Coalesce_Disabled()
{
	static const bool disabled = GgcFlags::Enabled(GgcFlag_NoSortCoalesce);
	return disabled;
}

static inline bool Is_Coalescable_Additive(const RenderStateStruct& rs)
{
	return rs.shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_DISABLE
		&& rs.shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE
		&& rs.shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE;
}

static inline unsigned long long Sort_Fnv1a(const void* data, unsigned len, unsigned long long hash)
{
	const unsigned char* p = static_cast<const unsigned char*>(data);
	for (unsigned i = 0; i < len; ++i)
	{
		hash ^= p[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

// Cheap key for grouping; equal state always yields an equal key. A key
// collision between differing states only places them adjacently - the draw
// loop still splits them with the exact Render_State_Matches test, so the
// reorder can never merge states that do not truly match.
static unsigned long long Sort_Node_State_Key(const RenderStateStruct& rs)
{
	unsigned long long hash = 1469598103934665603ULL;
	const unsigned int shaderBits = rs.shader.Get_Bits();
	hash = Sort_Fnv1a(&shaderBits, sizeof(shaderBits), hash);
	hash = Sort_Fnv1a(&rs.sorted_draw_flags, sizeof(rs.sorted_draw_flags), hash);
	const void* material = rs.material;
	hash = Sort_Fnv1a(&material, sizeof(material), hash);
	for (int texture_index = 0; texture_index < g_renderBackend->Get_Max_Texture_Stages(); ++texture_index)
	{
		const void* texture = rs.Textures[texture_index];
		hash = Sort_Fnv1a(&texture, sizeof(texture), hash);
	}
	hash = Sort_Fnv1a(&rs.world, sizeof(rs.world), hash);
	return hash;
}

static unsigned Count_Sorted_Runs(const TempIndexStruct* tis, unsigned tri_count)
{
	if (tri_count == 0)
	{
		return 0;
	}
	unsigned runs = 1;
	SortingNodeStruct* state = overlapping_nodes[tis[0].idx];
	for (unsigned i = 1; i < tri_count; ++i)
	{
		SortingNodeStruct* next_state = overlapping_nodes[tis[i].idx];
		if (!Render_State_Matches(state->sorting_state, next_state->sorting_state))
		{
			++runs;
			state = next_state;
		}
	}
	return runs;
}

static unsigned long long* sorted_packet_diag_segment_keys;
static unsigned sorted_packet_diag_segment_key_count;

static void Emit_Sorted_Packet_Collector_Diag(const TempIndexStruct* tis, unsigned tri_count, unsigned draw_runs)
{
	if (!g_renderBackend->Has_Shader_Pipeline() || !Sorted_Packet_Collector_Diag_Enabled())
	{
		return;
	}

	static unsigned flush_count = 0;
	if (flush_count >= Sorted_Packet_Collector_Diag_Max_Flushes())
	{
		return;
	}

	if (tri_count > sorted_packet_diag_segment_key_count)
	{
		delete[] sorted_packet_diag_segment_keys;
		sorted_packet_diag_segment_key_count = tri_count;
		sorted_packet_diag_segment_keys = W3DNEWARRAY unsigned long long[sorted_packet_diag_segment_key_count];
	}

	unsigned candidate_tris = 0;
	unsigned fallback_tris = 0;
	unsigned additive_segments = 0;
	unsigned largest_additive_window = 0;
	unsigned estimated_grouped_submits = 0;
	unsigned largest_fallback_window = 0;
	unsigned estimated_fallback_state_keys = 0;
	unsigned barrier_count = 0;
	unsigned fallback_packets = 0;
	unsigned source_pointgroup_tris = 0;
	unsigned source_streak_tris = 0;
	unsigned source_other_tris = 0;
	unsigned candidate_draw_runs = 0;
	unsigned fallback_draw_runs = 0;
	unsigned run_break_candidate_to_candidate = 0;
	unsigned run_break_fallback_to_fallback = 0;
	unsigned run_break_mixed_candidate_fallback = 0;
	unsigned run_break_reasons[BGFX_SORTED_PACKET_BREAK_COUNT] = {};
	unsigned texture_stage_breaks[RB_MAX_TEXTURE_STAGES] = {};
	unsigned texture0_same_name_breaks = 0;
	unsigned fallback_reasons[BGFX_SORTED_PACKET_FALLBACK_UNKNOWN + 1] = {};
	unsigned fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_COUNT] = {};
	unsigned pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_COUNT] = {};

	for (unsigned a = 0; a < tri_count; ++a)
	{
		SortingNodeStruct* node = overlapping_nodes[tis[a].idx];
		switch (node->packet_diag_source)
		{
			case BGFX_SORTED_PACKET_SOURCE_POINT_GROUP: ++source_pointgroup_tris; break;
			case BGFX_SORTED_PACKET_SOURCE_STREAK: ++source_streak_tris; break;
			default: ++source_other_tris; break;
		}
		if (node->packet_diag_fallback != BGFX_SORTED_PACKET_FALLBACK_NONE)
		{
			const unsigned reason = node->packet_diag_fallback <= BGFX_SORTED_PACKET_FALLBACK_UNKNOWN
				? node->packet_diag_fallback
				: BGFX_SORTED_PACKET_FALLBACK_UNKNOWN;
			++fallback_reasons[reason];
			const BgfxSortedPacketDiagBlendClass blendClass = Classify_Sorted_Packet_Diag_Blend(node->sorting_state);
			++fallback_blend_tris[blendClass];
			if (node->packet_diag_source == BGFX_SORTED_PACKET_SOURCE_POINT_GROUP)
			{
				++pointgroup_fallback_blend_tris[blendClass];
			}
		}
	}

	if (tri_count > 0)
	{
		SortingNodeStruct* run_node = overlapping_nodes[tis[0].idx];
		for (unsigned a = 1; a < tri_count; ++a)
		{
			SortingNodeStruct* next_node = overlapping_nodes[tis[a].idx];
			const unsigned break_mask = Sorted_State_Break_Mask(run_node->sorting_state, next_node->sorting_state);
			if (break_mask != 0)
			{
				for (unsigned reason = 0; reason < BGFX_SORTED_PACKET_BREAK_COUNT; ++reason)
				{
					if ((break_mask & (1u << reason)) != 0)
					{
						++run_break_reasons[reason];
					}
				}
				const unsigned texture_stage_mask = Sorted_State_Texture_Break_Mask(run_node->sorting_state, next_node->sorting_state);
				for (unsigned stage = 0; stage < RB_MAX_TEXTURE_STAGES; ++stage)
				{
					if ((texture_stage_mask & (1u << stage)) != 0)
					{
						++texture_stage_breaks[stage];
					}
				}
				if ((texture_stage_mask & 1u) != 0)
				{
					const char* left_name = Sorted_Texture_Full_Path(run_node->sorting_state, 0);
					const char* right_name = Sorted_Texture_Full_Path(next_node->sorting_state, 0);
					if (left_name != nullptr && right_name != nullptr && stricmp(left_name, right_name) == 0)
					{
						++texture0_same_name_breaks;
					}
				}
				const bool run_candidate = run_node->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE;
				const bool next_candidate = next_node->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE;
				if (run_candidate && next_candidate)
				{
					++run_break_candidate_to_candidate;
				}
				else if (!run_candidate && !next_candidate)
				{
					++run_break_fallback_to_fallback;
				}
				else
				{
					++run_break_mixed_candidate_fallback;
				}
				if (run_node->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE)
				{
					++candidate_draw_runs;
				}
				else
				{
					++fallback_draw_runs;
				}
				run_node = next_node;
			}
		}
		if (run_node->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE)
		{
			++candidate_draw_runs;
		}
		else
		{
			++fallback_draw_runs;
		}
	}

	unsigned i = 0;
	while (i < tri_count)
	{
		SortingNodeStruct* node = overlapping_nodes[tis[i].idx];

		if (node->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE)
		{
			const unsigned segment_start = i;
			do
			{
				++i;
			}
			while (i < tri_count && overlapping_nodes[tis[i].idx]->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE);

			const unsigned segment_end = i;
			const unsigned segment_length = segment_end - segment_start;
			candidate_tris += segment_length;
			++additive_segments;
			largest_additive_window = std::max(largest_additive_window, segment_length);

			unsigned unique_keys = 0;
			for (unsigned a = segment_start; a < segment_end; ++a)
			{
				const unsigned long long key = Sort_Node_State_Key(overlapping_nodes[tis[a].idx]->sorting_state);
				bool found = false;
				for (unsigned k = 0; k < unique_keys; ++k)
				{
					if (sorted_packet_diag_segment_keys[k] == key)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					sorted_packet_diag_segment_keys[unique_keys++] = key;
				}
			}
			estimated_grouped_submits += unique_keys;
			continue;
		}

		++barrier_count;
		++fallback_packets;
		const unsigned segment_start = i;
		while (i < tri_count)
		{
			SortingNodeStruct* fallback_node = overlapping_nodes[tis[i].idx];
			++fallback_tris;
			++i;
			if (i >= tri_count || overlapping_nodes[tis[i].idx]->packet_diag_fallback == BGFX_SORTED_PACKET_FALLBACK_NONE)
			{
				break;
			}
		}
		const unsigned segment_end = i;
		const unsigned segment_length = segment_end - segment_start;
		largest_fallback_window = std::max(largest_fallback_window, segment_length);

		unsigned unique_keys = 0;
		for (unsigned a = segment_start; a < segment_end; ++a)
		{
			const unsigned long long key = Sort_Node_State_Key(overlapping_nodes[tis[a].idx]->sorting_state);
			bool found = false;
			for (unsigned k = 0; k < unique_keys; ++k)
			{
				if (sorted_packet_diag_segment_keys[k] == key)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				sorted_packet_diag_segment_keys[unique_keys++] = key;
			}
		}
		estimated_fallback_state_keys += unique_keys;
	}

	if (FILE* diag = std::fopen("ggc_sorted_packet_collector_diag.txt", "a"))
	{
		std::fprintf(diag,
			"flush=%u nodes=%u tris=%u draw_runs=%u candidate_draw_runs=%u fallback_draw_runs=%u run_break_candidate_candidate=%u run_break_fallback_fallback=%u run_break_mixed=%u break_shader=%u break_flags=%u break_material=%u break_texture=%u break_texture0=%u break_texture1=%u break_texture2=%u break_texture3=%u break_texture0_same_name=%u break_world=%u break_view=%u break_light_enable=%u break_light_payload=%u candidate_packets=%u fallback_packets=%u barriers=%u candidate_tris=%u fallback_tris=%u additive_segments=%u largest_additive_window=%u estimated_grouped_submits=%u largest_fallback_window=%u estimated_fallback_state_keys=%u pointgroup_tris=%u streak_tris=%u other_tris=%u fallback_depth_write=%u fallback_alpha_test=%u fallback_non_additive_blend=%u fallback_unknown=%u fallback_blend_alpha=%u fallback_blend_multiply=%u fallback_blend_screen=%u fallback_blend_src_alpha_one=%u fallback_blend_other=%u pointgroup_fallback_blend_alpha=%u pointgroup_fallback_blend_multiply=%u pointgroup_fallback_blend_screen=%u pointgroup_fallback_blend_src_alpha_one=%u pointgroup_fallback_blend_other=%u\n",
			++flush_count,
			overlapping_node_count,
			tri_count,
			draw_runs,
			candidate_draw_runs,
			fallback_draw_runs,
			run_break_candidate_to_candidate,
			run_break_fallback_to_fallback,
			run_break_mixed_candidate_fallback,
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_SHADER],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_FLAGS],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_MATERIAL],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_TEXTURE],
			texture_stage_breaks[0],
			texture_stage_breaks[1],
			texture_stage_breaks[2],
			texture_stage_breaks[3],
			texture0_same_name_breaks,
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_WORLD],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_VIEW],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_LIGHT_ENABLE],
			run_break_reasons[BGFX_SORTED_PACKET_BREAK_LIGHT_PAYLOAD],
			estimated_grouped_submits,
			fallback_packets,
			barrier_count,
			candidate_tris,
			fallback_tris,
			additive_segments,
			largest_additive_window,
			estimated_grouped_submits,
			largest_fallback_window,
			estimated_fallback_state_keys,
			source_pointgroup_tris,
			source_streak_tris,
			source_other_tris,
			fallback_reasons[BGFX_SORTED_PACKET_FALLBACK_DEPTH_WRITE],
			fallback_reasons[BGFX_SORTED_PACKET_FALLBACK_ALPHA_TEST],
			fallback_reasons[BGFX_SORTED_PACKET_FALLBACK_NON_ADDITIVE_BLEND],
			fallback_reasons[BGFX_SORTED_PACKET_FALLBACK_UNKNOWN],
			fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_ALPHA],
			fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_MULTIPLY],
			fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_SCREEN],
			fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_SRC_ALPHA_ONE],
			fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_OTHER],
			pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_ALPHA],
			pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_MULTIPLY],
			pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_SCREEN],
			pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_SRC_ALPHA_ONE],
			pointgroup_fallback_blend_tris[BGFX_SORTED_PACKET_BLEND_OTHER]);
		std::fclose(diag);
	}
}

static TempIndexStruct* coalesce_scratch;
static unsigned coalesce_scratch_count;
static bool* coalesce_consumed;
static unsigned coalesce_consumed_count;
static unsigned long long* coalesce_node_key;
static bool* coalesce_node_add;
static unsigned coalesce_node_capacity;

static void Coalesce_Sorted_Pool(TempIndexStruct* tis, unsigned tri_count)
{
	if (tri_count < 2)
	{
		return;
	}

	if (overlapping_node_count > coalesce_node_capacity)
	{
		delete[] coalesce_node_key;
		delete[] coalesce_node_add;
		coalesce_node_capacity = overlapping_node_count;
		coalesce_node_key = W3DNEWARRAY unsigned long long[coalesce_node_capacity];
		coalesce_node_add = W3DNEWARRAY bool[coalesce_node_capacity];
	}
	for (unsigned node_id = 0; node_id < overlapping_node_count; ++node_id)
	{
		const RenderStateStruct& rs = overlapping_nodes[node_id]->sorting_state;
		coalesce_node_add[node_id] = Is_Coalescable_Additive(rs);
		coalesce_node_key[node_id] = coalesce_node_add[node_id] ? Sort_Node_State_Key(rs) : 0ULL;
	}

	if (tri_count > coalesce_scratch_count)
	{
		delete[] coalesce_scratch;
		coalesce_scratch_count = tri_count;
		coalesce_scratch = W3DNEWARRAY TempIndexStruct[coalesce_scratch_count];
	}
	if (tri_count > coalesce_consumed_count)
	{
		delete[] coalesce_consumed;
		coalesce_consumed_count = tri_count;
		coalesce_consumed = W3DNEWARRAY bool[coalesce_consumed_count];
	}

	unsigned i = 0;
	while (i < tri_count)
	{
		if (!coalesce_node_add[tis[i].idx])
		{
			++i;
			continue;
		}

		// Maximal segment of consecutive coalescable additive triangles.
		unsigned segment_end = i;
		while (segment_end < tri_count && coalesce_node_add[tis[segment_end].idx])
		{
			++segment_end;
		}

		// Stable first-appearance grouping of [i, segment_end) by node key.
		const unsigned segment_length = segment_end - i;
		for (unsigned a = i; a < segment_end; ++a)
		{
			coalesce_consumed[a] = false;
		}
		unsigned out = 0;
		for (unsigned a = i; a < segment_end; ++a)
		{
			if (coalesce_consumed[a])
			{
				continue;
			}
			const unsigned long long key = coalesce_node_key[tis[a].idx];
			coalesce_scratch[out++] = tis[a];
			coalesce_consumed[a] = true;
			for (unsigned b = a + 1; b < segment_end; ++b)
			{
				if (!coalesce_consumed[b] && coalesce_node_key[tis[b].idx] == key)
				{
					coalesce_scratch[out++] = tis[b];
					coalesce_consumed[b] = true;
				}
			}
		}
		memcpy(tis + i, coalesce_scratch, segment_length * sizeof(TempIndexStruct));

		i = segment_end;
	}
}

// ----------------------------------------------------------------------------

static void Draw_Sorted_Run(unsigned start_index,
                            unsigned count_to_render,
                            SortingNodeStruct* state,
                            const DynamicVBAccessClass& dyn_vb_access,
                            const DynamicIBAccessClass& dyn_ib_access,
                            int array_page = -1)
{
	if (array_page >= 0)
	{
		g_renderBackend->Set_Sorted_Texture_Array_Page(array_page);
	}
	Apply_Render_State(state);
	if (!g_renderBackend->Has_Shader_Pipeline())
	{
		g_renderBackend->Set_Index_Buffer(dyn_ib_access, 0);
		g_renderBackend->Set_Vertex_Buffer(dyn_vb_access);
	}
	Log_Sort_Effect_Diag("draw-run", start_index, count_to_render, state);

	g_renderBackend->Draw_Triangles(
		start_index*3,
		count_to_render,
		0,
		overlapping_vertex_count);
	if (array_page >= 0)
	{
		g_renderBackend->Set_Sorted_Texture_Array_Page(-1);
	}
}

// ----------------------------------------------------------------------------

void SortingRendererClass::Flush_Sorting_Pool()
{
	if (!overlapping_node_count) return;
	Log_Sort_Effect_Diag("flush-start", 0, overlapping_polygon_count, overlapping_nodes[0]);

	SNAPSHOT_SAY(("SortingSystem - Flush"));

	// Fill dynamic index buffer with sorting index buffer vertices
	TempIndexStruct* tis=Get_Temp_Index_Array(overlapping_polygon_count);

	{
		unsigned vertexAllocCount = overlapping_vertex_count;
		if (!g_renderBackend->Has_Shader_Pipeline())
		{
			if (DynamicVBAccessClass::Get_Default_Vertex_Count() < DEFAULT_SORTING_VERTEX_COUNT)
				vertexAllocCount = DEFAULT_SORTING_VERTEX_COUNT;	//make sure that we force the DX8 dynamic vertex buffer to maximum size
			if (overlapping_vertex_count > vertexAllocCount)
				vertexAllocCount = overlapping_vertex_count;
			WWASSERT(DEFAULT_SORTING_VERTEX_COUNT == 1 || vertexAllocCount <= DEFAULT_SORTING_VERTEX_COUNT);
		}
		DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC,dynamic_fvf_type,vertexAllocCount/*overlapping_vertex_count*/);
		GGCRenderProfile::Begin(GGCRenderProfile::SORT_POOL_BUILD);
		{
			DynamicVBAccessClass::WriteLockClass lock(&dyn_vb_access);
			VertexFormatXYZNDUV2* dest_verts=(VertexFormatXYZNDUV2 *)lock.Get_Formatted_Vertex_Array();

			unsigned polygon_array_offset=0;
			unsigned vertex_array_offset=0;
			s_sortedNodeArrayPage.assign(overlapping_node_count, -1);
			for (unsigned node_id=0;node_id<overlapping_node_count;++node_id) {
				SortingNodeStruct* state=overlapping_nodes[node_id];
				VertexFormatXYZNDUV2* src_verts=nullptr;
				SortingVertexBufferClass* vertex_buffer=static_cast<SortingVertexBufferClass*>(state->sorting_state.vertex_buffers[0]);
				WWASSERT(vertex_buffer);
				src_verts=vertex_buffer->VertexBuffer;
				WWASSERT(src_verts);
				src_verts+=state->sorting_state.vba_offset;
				src_verts+=state->sorting_state.index_base_offset;
				src_verts+=state->min_vertex_index;

				// If you have a crash in here and "dest_verts" points to illegal memory area,
				// it is because D3D is in illegal state, and the only known cure is rebooting.
				// This illegal state is usually caused by Quake3-engine powered games such as MOHAA.
				memcpy(dest_verts, src_verts, sizeof(VertexFormatXYZNDUV2)*state->vertex_count);
				{
					int arrayPage = -1;
					int arrayLayer = -1;
					float arrayScaleU = 1.0f;
					float arrayScaleV = 1.0f;
					if (g_renderBackend->Get_Sorted_Texture_Array_Slot(state->sorting_state, &arrayPage, &arrayLayer, &arrayScaleU, &arrayScaleV))
					{
						// Second UV channel = page-region UVs, normal z = layer.
						// Both are dead data for the eligible unlit single-stage
						// profile, and only the array program reads them.
						const float layerCoord = static_cast<float>(arrayLayer) + 0.5f;
						for (unsigned stamped_vertex = 0; stamped_vertex < state->vertex_count; ++stamped_vertex)
						{
							dest_verts[stamped_vertex].u2 = dest_verts[stamped_vertex].u1 * arrayScaleU;
							dest_verts[stamped_vertex].v2 = dest_verts[stamped_vertex].v1 * arrayScaleV;
							dest_verts[stamped_vertex].nz = layerCoord;
						}
					}
					s_sortedNodeArrayPage[node_id] = arrayPage;
					++s_sortedArrayDbgNodes;
					if (arrayPage >= 0)
					{
						++s_sortedArrayDbgEligible;
					}
				}
				dest_verts += state->vertex_count;

				const Matrix4x4 mtx = Get_Sorted_World_View_Matrix(state->sorting_state);

				unsigned short* indices=nullptr;
				SortingIndexBufferClass* index_buffer=static_cast<SortingIndexBufferClass*>(state->sorting_state.index_buffer);
				WWASSERT(index_buffer);
				indices=index_buffer->index_buffer;
				WWASSERT(indices);
				indices+=state->start_index;
				indices+=state->sorting_state.iba_offset;

					if (mtx[0][2] == 0.0f && mtx[1][2] == 0.0f && mtx[3][2] == 0.0f && mtx[2][2] == 1.0f) {
					// The common case for particle systems.
					for (int i=0;i<state->polygon_count;++i) {
						unsigned short idx1=indices[i*3]-state->min_vertex_index;
						unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
						unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
						WWASSERT(idx1<state->vertex_count);
						WWASSERT(idx2<state->vertex_count);
						WWASSERT(idx3<state->vertex_count);
						const VertexFormatXYZNDUV2 *v1 = src_verts + idx1;
						const VertexFormatXYZNDUV2 *v2 = src_verts + idx2;
						const VertexFormatXYZNDUV2 *v3 = src_verts + idx3;
						unsigned array_index=i+polygon_array_offset;
						WWASSERT(array_index<overlapping_polygon_count);
						TempIndexStruct *tis_ptr = tis + array_index;
						tis_ptr->tri.i = idx1 + vertex_array_offset;
						tis_ptr->tri.j = idx2 + vertex_array_offset;
						tis_ptr->tri.k = idx3 + vertex_array_offset;
						tis_ptr->idx = node_id;
						tis_ptr->z = (v1->z + v2->z + v3->z)/3.0f;
						DEBUG_ASSERTCRASH((! _isnan(tis_ptr->z) && _finite(tis_ptr->z)), ("Triangle has invalid center"));
					}
				} else {
					for (int i=0;i<state->polygon_count;++i) {
						unsigned short idx1=indices[i*3]-state->min_vertex_index;
						unsigned short idx2=indices[i*3+1]-state->min_vertex_index;
						unsigned short idx3=indices[i*3+2]-state->min_vertex_index;
						WWASSERT(idx1<state->vertex_count);
						WWASSERT(idx2<state->vertex_count);
						WWASSERT(idx3<state->vertex_count);
						const VertexFormatXYZNDUV2 *v1 = src_verts + idx1;
						const VertexFormatXYZNDUV2 *v2 = src_verts + idx2;
						const VertexFormatXYZNDUV2 *v3 = src_verts + idx3;
						unsigned array_index=i+polygon_array_offset;
						WWASSERT(array_index<overlapping_polygon_count);
						TempIndexStruct *tis_ptr = tis + array_index;
						tis_ptr->tri.i = idx1 + vertex_array_offset;
						tis_ptr->tri.j = idx2 + vertex_array_offset;
						tis_ptr->tri.k = idx3 + vertex_array_offset;
						tis_ptr->idx = node_id;
						tis_ptr->z = (mtx[0][2]*(v1->x + v2->x + v3->x) +
													mtx[1][2]*(v1->y + v2->y + v3->y) +
													mtx[2][2]*(v1->z + v2->z + v3->z))/3.0f + mtx[3][2];
						DEBUG_ASSERTCRASH((! _isnan(tis_ptr->z) && _finite(tis_ptr->z)), ("Triangle has invalid center"));
					}
				}

				state->min_vertex_index=vertex_array_offset;

				polygon_array_offset+=state->polygon_count;
				vertex_array_offset+=state->vertex_count;
			}
		}
		GGCRenderProfile::End(GGCRenderProfile::SORT_POOL_BUILD);

		{
			GGC_RPROFILE(SORT_POOL_SORT);
			Sort(tis, tis + overlapping_polygon_count);

			if (g_renderBackend->Has_Shader_Pipeline() && !Sort_Coalesce_Disabled())
			{
				const bool diag = Should_Log_Sort_Effect_Diag();
				const unsigned runs_before = diag ? Count_Sorted_Runs(tis, overlapping_polygon_count) : 0;
				Coalesce_Sorted_Pool(tis, overlapping_polygon_count);
				if (diag)
				{
					const unsigned runs_after = Count_Sorted_Runs(tis, overlapping_polygon_count);
					if (FILE* diagFile = std::fopen("ggc_sort_effect_diag.txt", "a"))
					{
						std::fprintf(diagFile,
							"coalesce nodes=%u tris=%u runs_before=%u runs_after=%u saved=%d\n",
							overlapping_node_count, overlapping_polygon_count, runs_before, runs_after,
							(int)runs_before - (int)runs_after);
						std::fclose(diagFile);
					}
				}
			}
			if (g_renderBackend->Has_Shader_Pipeline() && Sorted_Packet_Collector_Diag_Enabled())
			{
				Emit_Sorted_Packet_Collector_Diag(tis, overlapping_polygon_count, Count_Sorted_Runs(tis, overlapping_polygon_count));
			}
		}

		{
			GGC_RPROFILE(SORT_POOL_DRAW);
			// TheSuperHackers @fix stephanmeesters 10/06/2026
			// Split rendering into chunks to prevent a crash when exceeding the 16-bit index buffer limit.
			constexpr const unsigned MAX_INDEX_CHUNK = 65535;

			// route bgfx submits through the dedicated sort view
			// id for the rest of this flush. No-op on DX8Backend.
			g_renderBackend->Begin_Sorted_Batch_Pass();

			unsigned chunkOffset = 0;
			while (chunkOffset < overlapping_polygon_count)
			{
				unsigned chunkCount = overlapping_polygon_count - chunkOffset;
				if (chunkCount * 3 > MAX_INDEX_CHUNK) {
					chunkCount = MAX_INDEX_CHUNK / 3;
				}
				const unsigned chunkEnd = chunkOffset + chunkCount;

				DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC,chunkCount*3);
				{
					DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
					ShortVectorIStruct* sorted_polygon_index_array=(ShortVectorIStruct*)lock.Get_Index_Array();

					for (unsigned a=0;a<chunkCount;++a) {
						sorted_polygon_index_array[a]=tis[chunkOffset + a].tri;
					}
				}

				// Set index buffer and render!

				g_renderBackend->Set_Index_Buffer(dyn_ib_access, 0); // Override with this buffer (do something to prevent need for this!)
				g_renderBackend->Set_Vertex_Buffer(dyn_vb_access); // Override with this buffer (do something to prevent need for this!)
				Log_Sort_Effect_Diag("buffers-set", 0, overlapping_polygon_count, overlapping_nodes[0]);

				g_renderBackend->Apply_Render_State_Changes();

				unsigned count_to_render=1;
				unsigned start_index=0;
				SortingNodeStruct* state=overlapping_nodes[tis[chunkOffset].idx];
				int run_array_page = s_sortedNodeArrayPage[tis[chunkOffset].idx];
				bool run_array_merged = false;
				for (unsigned i=chunkOffset + 1;i<chunkEnd;++i) {
					SortingNodeStruct* next_state=overlapping_nodes[tis[i].idx];
					if (!Render_State_Matches(state->sorting_state,next_state->sorting_state))
					{
						// Adjacent runs that differ only by their stage-0 texture keep
						// accumulating into one texture-array draw: the z order is
						// untouched, the per-vertex layer picks each triangle's texture.
						const int next_array_page = s_sortedNodeArrayPage[tis[i].idx];
						if (run_array_page >= 0
							&& next_array_page == run_array_page
							&& Render_State_Matches_Except_Stage0_Texture(state->sorting_state, next_state->sorting_state))
						{
							run_array_merged = true;
							state=next_state;
							++s_sortedArrayDbgMerges;
						}
						else
						{
							if (run_array_page >= 0 && next_array_page == run_array_page)
							{
								static const bool s_traceBreaks = GgcFlags::Enabled(GgcFlag_Trace);
								if (s_traceBreaks)
								{
									extern unsigned s_sortedArrayBreakMaskCounts[256];
									const unsigned mask = Sorted_State_Break_Mask(state->sorting_state, next_state->sorting_state) & 0xFF;
									++s_sortedArrayBreakMaskCounts[mask];
								}
							}
							Draw_Sorted_Run(start_index,count_to_render,state,dyn_vb_access,dyn_ib_access,
								run_array_merged ? run_array_page : -1);

							count_to_render=0;
							start_index=i - chunkOffset;
							state=next_state;
							run_array_page = next_array_page;
							run_array_merged = false;
						}
					}
					count_to_render++;	//keep track of number of polygons of same kind
				}

				// Render any remaining polygons...
				if (count_to_render) {
					Draw_Sorted_Run(start_index,count_to_render,state,dyn_vb_access,dyn_ib_access,
						run_array_merged ? run_array_page : -1);
				}

				chunkOffset += chunkCount;
			}

			// sort flush complete, resume routing bgfx submits
			// through the main engine view id.
			g_renderBackend->End_Sorted_Batch_Pass();
			if (Should_Log_Sort_Effect_Diag() || GgcFlags::Enabled(GgcFlag_Trace))
			{
				if ((++s_sortedArrayDbgFlushes % 200) == 0)
				{
					std::fprintf(stderr,
						"[ggc] sorted-array dbg: flushes=%u nodes=%u eligible=%u merges=%u breaks:",
						s_sortedArrayDbgFlushes, s_sortedArrayDbgNodes,
						s_sortedArrayDbgEligible, s_sortedArrayDbgMerges);
					for (unsigned mask = 0; mask < 256; ++mask)
					{
						if (s_sortedArrayBreakMaskCounts[mask] != 0)
						{
							std::fprintf(stderr, " 0x%x=%u", mask, s_sortedArrayBreakMaskCounts[mask]);
						}
					}
					std::fprintf(stderr, "\n");
					std::fflush(stderr);
				}
			}
		}
	}

	// Release all references and return nodes back to the clean list for the frame...
	for (unsigned node_id=0;node_id<overlapping_node_count;++node_id)
	{
		SortingNodeStruct* state=overlapping_nodes[node_id];
		Release_Refs(state);
		clean_list.push_front(state);
	}
	overlapping_node_count=0;
	overlapping_polygon_count=0;
	overlapping_vertex_count=0;

	SNAPSHOT_SAY(("SortingSystem - Done flushing"));

}

// ----------------------------------------------------------------------------

void SortingRendererClass::Flush()
{
	WWPROFILE("SortingRenderer::Flush");
	Matrix4x4 old_view;
	Matrix4x4 old_world;
	g_renderBackend->Get_Transform(RB_TRANSFORM_VIEW, old_view);
	g_renderBackend->Get_Transform(RB_TRANSFORM_WORLD, old_world);
	static const bool s_probeNoSortFlush = GgcFlags::Enabled(GgcFlag_ProbeNoSortFlush);
	if (s_probeNoSortFlush) {
		while (!sorted_list.empty()) {
			SortingNodeStruct* state = sorted_list.front();
			sorted_list.pop_front();
			Release_Refs(state);
			clean_list.push_front(state);
		}
		for (unsigned node_id=0;node_id<overlapping_node_count;++node_id) {
			SortingNodeStruct* state=overlapping_nodes[node_id];
			Release_Refs(state);
			clean_list.push_front(state);
		}
		overlapping_node_count=0;
		overlapping_polygon_count=0;
		overlapping_vertex_count=0;
		total_sorting_vertices=0;
		DynamicIBAccessClass::_Reset(false);
		DynamicVBAccessClass::_Reset(false);
		g_renderBackend->Set_Transform(RB_TRANSFORM_VIEW,old_view);
		g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,old_world);
		return;
	}

	// TheSuperHackers @perf stephanmeesters 04/07/2026
	// Splice nodes that have no bounding information (Z=0.0) at the correct location into the sorted list.
	SortingNodeStructList::iterator node = sorted_list.begin();
	while (node != sorted_list.end() && (*node)->transformed_center.Z > 0.0f) {
		++node;
	}
	sorted_list.splice(node, unsorted_list);

	while (!sorted_list.empty()) {
		SortingNodeStruct* state = sorted_list.front();
		sorted_list.pop_front();

		if ((state->sorting_state.index_buffer_type==BUFFER_TYPE_SORTING || state->sorting_state.index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) &&
			(state->sorting_state.vertex_buffer_types[0]==BUFFER_TYPE_SORTING || state->sorting_state.vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING)) {
			Insert_To_Sorting_Pool(state);
		}
		else {
			Apply_Render_State(state);
			g_renderBackend->Set_Vertex_Buffer(state->sorting_state.vertex_buffers[0], 0);
			g_renderBackend->Set_Index_Buffer(state->sorting_state.index_buffer,
				state->sorting_state.index_base_offset);
			g_renderBackend->Set_Index_Buffer_Index_Offset(state->sorting_state.vba_offset);

			// Restore legacy DX8 render state: the backend calls above can
			// mutate vba_offset/iba_offset in the DX8 render-state cache.
			// Re-applying the saved state fixes this for the DX8 draw path;
			// bgfx treats this as a no-op.
			g_renderBackend->Restore_Legacy_Render_State_For_Sorted_Draw(state->sorting_state);

			// Use the sort view for transforms to avoid stomping view 1.
			g_renderBackend->Begin_Sorted_Batch_Pass();

			g_renderBackend->Draw_Triangles(state->start_index, state->polygon_count, state->min_vertex_index, state->vertex_count);
			g_renderBackend->End_Sorted_Batch_Pass();
			g_renderBackend->Release_Legacy_Render_State_For_Sorted_Draw();
			Release_Refs(state);
			clean_list.push_front(state);
		}
	}

	bool old_enable=g_renderBackend->Is_Triangle_Draw_Enabled();
	g_renderBackend->Set_Triangle_Draw_Enabled(_EnableTriangleDraw);
	Flush_Sorting_Pool();
	g_renderBackend->Set_Triangle_Draw_Enabled(old_enable);

	g_renderBackend->Set_Index_Buffer(nullptr, 0);
	g_renderBackend->Set_Vertex_Buffer(nullptr, 0);
	total_sorting_vertices=0;

	DynamicIBAccessClass::_Reset(false);
	DynamicVBAccessClass::_Reset(false);


	g_renderBackend->Set_Transform(RB_TRANSFORM_VIEW,old_view);
	g_renderBackend->Set_Transform(RB_TRANSFORM_WORLD,old_world);

}

// ----------------------------------------------------------------------------

void SortingRendererClass::Deinit()
{
	//
	//	Flush the sorted list
	//
	while (!sorted_list.empty()) {
		delete sorted_list.front();
		sorted_list.pop_front();
	}

	//
	//	Flush the unsorted list
	//
	while (!unsorted_list.empty()) {
		delete unsorted_list.front();
		unsorted_list.pop_front();
	}

	//
	//	Flush the clean list
	//
	while (!clean_list.empty()) {
		delete clean_list.front();
		clean_list.pop_front();
	}

	delete[] temp_index_array;
	temp_index_array=nullptr;
	temp_index_array_count=0;
}
