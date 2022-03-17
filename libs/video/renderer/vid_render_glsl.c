/*
	vid_render_glsl.c

	GLSL version of the renderer

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/GLSL/funcs.h"
#include "QF/GLSL/defines.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_main.h"
#include "QF/GLSL/qf_vid.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

gl_ctx_t *glsl_ctx;

static void
glsl_vid_render_choose_visual (void *data)
{
	glsl_ctx->choose_visual (glsl_ctx);
}

static void
glsl_vid_render_create_context (void *data)
{
	glsl_ctx->create_context (glsl_ctx);
}

static vid_model_funcs_t model_funcs = {
	sizeof (glsltex_t),
	glsl_Mod_LoadLighting,
	0,//Mod_SubdivideSurface,
	glsl_Mod_ProcessTexture,

	Mod_LoadIQM,
	Mod_LoadAliasModel,
	Mod_LoadSpriteModel,

	glsl_Mod_MakeAliasModelDisplayLists,
	glsl_Mod_LoadSkin,
	glsl_Mod_FinalizeAliasModel,
	glsl_Mod_LoadExternalSkins,
	glsl_Mod_IQMFinish,
	0,
	glsl_Mod_SpriteLoadFrames,

	Skin_SetColormap,
	Skin_SetSkin,
	glsl_Skin_SetupSkin,
	Skin_SetTranslation,
	glsl_Skin_ProcessTranslation,
	glsl_Skin_InitTranslations,
};

static void
glsl_vid_render_init (void)
{
	if (!vr_data.vid->vid_internal->sw_context) {
		Sys_Error ("Sorry, OpenGL (GLSL) not supported by this program.");
	}
	glsl_ctx = vr_data.vid->vid_internal->gl_context ();
	glsl_ctx->init_gl = GLSL_Init_Common;
	glsl_ctx->load_gl ();

	vr_data.vid->vid_internal->data = glsl_ctx;
	vr_data.vid->vid_internal->set_palette = GLSL_SetPalette;
	vr_data.vid->vid_internal->choose_visual = glsl_vid_render_choose_visual;
	vr_data.vid->vid_internal->create_context = glsl_vid_render_create_context;
	vr_funcs = &glsl_vid_render_funcs;
	m_funcs = &model_funcs;
}

static void
glsl_vid_render_shutdown (void)
{
}

static unsigned int GLErr_InvalidEnum;
static unsigned int GLErr_InvalidValue;
static unsigned int GLErr_InvalidOperation;
static unsigned int GLErr_OutOfMemory;
static unsigned int GLErr_Unknown;

static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfeglGetError ()) {
	case GL_NO_ERROR:
		return numerous;
		break;
	case GL_INVALID_ENUM:
		GLErr_InvalidEnum++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_VALUE:
		GLErr_InvalidValue++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_OPERATION:
		GLErr_InvalidOperation++;
		R_TestErrors (numerous++);
		break;
	case GL_OUT_OF_MEMORY:
		GLErr_OutOfMemory++;
		R_TestErrors (numerous++);
		break;
	default:
		GLErr_Unknown++;
		R_TestErrors (numerous++);
		break;
	}

	return numerous;
}

static void
R_DisplayErrors (void)
{
	if (GLErr_InvalidEnum)
		printf ("%d OpenGL errors: Invalid Enum!\n", GLErr_InvalidEnum);
	if (GLErr_InvalidValue)
		printf ("%d OpenGL errors: Invalid Value!\n", GLErr_InvalidValue);
	if (GLErr_InvalidOperation)
		printf ("%d OpenGL errors: Invalid Operation!\n", GLErr_InvalidOperation);
	if (GLErr_OutOfMemory)
		printf ("%d OpenGL errors: Out Of Memory!\n", GLErr_OutOfMemory);
	if (GLErr_Unknown)
		printf ("%d Unknown OpenGL errors!\n", GLErr_Unknown);
}

static void
R_ClearErrors (void)
{
	GLErr_InvalidEnum = 0;
	GLErr_InvalidValue = 0;
	GLErr_InvalidOperation = 0;
	GLErr_OutOfMemory = 0;
	GLErr_Unknown = 0;
}

static void
glsl_begin_frame (void)
{
	if (R_TestErrors (0))
		R_DisplayErrors ();
	R_ClearErrors ();

	if (glsl_ctx->begun) {
		glsl_ctx->begun = 0;
		glsl_ctx->end_rendering ();
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in glsl mode
	vr_data.scr_copyeverything = 1;

	qfeglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glsl_ctx->begun = 1;

	GLSL_Set2D ();
	GLSL_DrawReset ();

	if (r_refdef.vrect.x > 0) {
		int         rx = r_refdef.vrect.x + r_refdef.vrect.width;
		int         vh = vid.height - vr_data.lineadj;
		// left
		glsl_Draw_TileClear (0, 0, r_refdef.vrect.x, vh);
		// right
		glsl_Draw_TileClear (rx, 0, vid.width - rx, vh);
	}
	if (r_refdef.vrect.y > 0) {
		int         lx = r_refdef.vrect.x;
		int         ty = r_refdef.vrect.y;
		int         rx = r_refdef.vrect.x + r_refdef.vrect.width;
		int         by = r_refdef.vrect.y + r_refdef.vrect.height;
		int         vh = vid.height - vr_data.lineadj;
		// top
		glsl_Draw_TileClear (lx, 0, rx, ty);
		// bottom
		glsl_Draw_TileClear (lx, by, r_refdef.vrect.width, vh - by);
	}
}

static void
glsl_render_view (void)
{
	glsl_R_RenderView ();
}

static void
glsl_set_2d (int scaled)
{
	if (scaled) {
		GLSL_Set2DScaled ();
	} else {
		GLSL_Set2D ();
	}
}

static void
glsl_end_frame (void)
{
	GLSL_FlushText ();
	GLSL_End2D ();
	qfeglFlush ();
}

vid_render_funcs_t glsl_vid_render_funcs = {
	glsl_vid_render_init,
	glsl_Draw_Character,
	glsl_Draw_String,
	glsl_Draw_nString,
	glsl_Draw_AltString,
	glsl_Draw_ConsoleBackground,
	glsl_Draw_Crosshair,
	glsl_Draw_CrosshairAt,
	glsl_Draw_TileClear,
	glsl_Draw_Fill,
	glsl_Draw_TextBox,
	glsl_Draw_FadeScreen,
	glsl_Draw_BlendScreen,
	glsl_Draw_CachePic,
	glsl_Draw_UncachePic,
	glsl_Draw_MakePic,
	glsl_Draw_DestroyPic,
	glsl_Draw_PicFromWad,
	glsl_Draw_Pic,
	glsl_Draw_Picf,
	glsl_Draw_SubPic,

	glsl_SCR_CaptureBGR,

	glsl_ParticleSystem,
	glsl_R_Init,
	glsl_R_ClearState,
	glsl_R_LoadSkys,
	glsl_R_NewMap,
	glsl_R_LineGraph,
	glsl_R_ViewChanged,
	glsl_begin_frame,
	glsl_render_view,
	glsl_set_2d,
	glsl_end_frame,
	&model_funcs
};

static general_funcs_t plugin_info_general_funcs = {
	.shutdown = glsl_vid_render_shutdown,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.vid_render = &glsl_vid_render_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.vid_render = &vid_render_data,
};

static plugin_t plugin_info = {
	qfp_vid_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"GLSL Renderer",
	"Copyright (C) 1996-1997  Id Software, Inc.\n"
	"Copyright (C) 1999-2012  contributors of the QuakeForge project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(vid_render, glsl)
{
	return &plugin_info;
}
