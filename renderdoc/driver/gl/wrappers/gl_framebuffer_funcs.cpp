/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "common/common.h"
#include "serialise/string_utils.h"
#include "../gl_driver.h"

bool WrappedOpenGL::Serialise_glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenFramebuffers(1, &real);
		
		GLResource res = FramebufferRes(GetCtx(), real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	m_Real.glGenFramebuffers(n, framebuffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_FRAMEBUFFERS);
				Serialise_glGenFramebuffers(1, framebuffers+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferTextureEXT(0, Attach, res.name, Level);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferTextureEXT(fbres.name, Attach, res.name, Level);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
		Serialise_glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture(target, attachment, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
		Serialise_glNamedFramebufferTextureEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferTexture1DEXT(0, Attach, TexTarget, res.name, Level);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferTexture1DEXT(fbres.name, Attach, TexTarget, res.name, Level);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX1D);
		Serialise_glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture1D(target, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX1D);
		Serialise_glNamedFramebufferTexture1DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferTexture2DEXT(0, Attach, TexTarget, res.name, Level);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferTexture2DEXT(fbres.name, Attach, TexTarget, res.name, Level);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
		Serialise_glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture2D(target, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
		Serialise_glNamedFramebufferTexture2DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, Zoffset, zoffset);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferTexture3DEXT(0, Attach, TexTarget, res.name, Level, Zoffset);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferTexture3DEXT(fbres.name, Attach, TexTarget, res.name, Level, Zoffset);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	m_Real.glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX3D);
		Serialise_glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	m_Real.glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX3D);
		Serialise_glNamedFramebufferTexture3DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, textarget, texture, level, zoffset);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}
bool WrappedOpenGL::Serialise_glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(GLenum, RendBufTarget, renderbuffertarget);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer)));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferRenderbufferEXT(0, Attach, RendBufTarget, res.name);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferRenderbufferEXT(fbres.name, Attach, RendBufTarget, res.name);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	m_Real.glNamedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_RENDBUF);
		Serialise_glNamedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);
		
		if(m_State == WRITING_IDLE)
		{
			if(renderbuffer != 0 && GetResourceManager()->HasResourceRecord(RenderbufferRes(GetCtx(), renderbuffer)))
				record->AddParent(GetResourceManager()->GetResourceRecord(RenderbufferRes(GetCtx(), renderbuffer)));
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	m_Real.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_RENDBUF);
		Serialise_glNamedFramebufferRenderbufferEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					      attachment, renderbuffertarget, renderbuffer);
		
		if(m_State == WRITING_IDLE)
		{
			if(renderbuffer != 0 && GetResourceManager()->HasResourceRecord(RenderbufferRes(GetCtx(), renderbuffer)))
				record->AddParent(GetResourceManager()->GetResourceRecord(RenderbufferRes(GetCtx(), renderbuffer)));
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, Layer, layer);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			m_Real.glNamedFramebufferTextureLayerEXT(0, Attach, res.name, Level, Layer);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferTextureLayerEXT(fbres.name, Attach, res.name, Level, Layer);
		}

		if(m_State == READING)
		{
			m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= eTextureCreate_RTV;
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	m_Real.glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
		Serialise_glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	m_Real.glFramebufferTextureLayer(target, attachment, texture, level, layer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
		Serialise_glNamedFramebufferTextureLayerEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, texture, level, layer);
		
		if(m_State == WRITING_IDLE)
		{
			if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
			{
				ResourceRecord *texrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
				record->AddParent(texrecord);
				GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
			}
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State == READING)
	{
		if(fbid != ResourceId())
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			m_Real.glNamedFramebufferParameteriEXT(fbres.name, PName, Param);
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
	m_Real.glNamedFramebufferParameteriEXT(framebuffer, pname, param);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_PARAM);
		Serialise_glNamedFramebufferParameteriEXT(framebuffer, pname, param);
		
		record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
	m_Real.glFramebufferParameteri(target, pname, param);

	if(m_State >= WRITING)
	{
		GLResourceRecord *record = NULL;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		if(record == NULL) return;

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_PARAM);
		Serialise_glNamedFramebufferParameteriEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name, pname, param);

		record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glReadBuffer(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, m, mode);
	SERIALISE_ELEMENT(ResourceId, id, GetCtxData().m_ReadFramebufferRecord ? GetCtxData().m_ReadFramebufferRecord->GetResourceID() : ResourceId());

	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);
		}
		else
		{
			m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, m_FakeBB_FBO);
		}

		m_Real.glReadBuffer(m);
	}
	
	return true;
}

void WrappedOpenGL::glReadBuffer(GLenum mode)
{
	m_Real.glReadBuffer(mode);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(READ_BUFFER);
		Serialise_glReadBuffer(mode);

		if(m_State == WRITING_IDLE)
		{
			if(GetCtxData().m_ReadFramebufferRecord)
			{
				Chunk *last = GetCtxData().m_ReadFramebufferRecord->GetLastChunk();
				if(last->GetChunkType() == READ_BUFFER)
				{
					delete last;
					GetCtxData().m_ReadFramebufferRecord->PopChunk();
				}
				GetCtxData().m_ReadFramebufferRecord->AddChunk(scope.Get());
			}
			else
				m_DeviceRecord->AddChunk(scope.Get());
		}
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));

	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
		{
			m_Real.glBindFramebuffer(Target, m_FakeBB_FBO);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(Id);
			m_Real.glBindFramebuffer(Target, res.name);
		}
	}

	return true;
}

void WrappedOpenGL::glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_FRAMEBUFFER);
		Serialise_glBindFramebuffer(target, framebuffer);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	if(framebuffer == 0 && m_State < WRITING)
		framebuffer = m_FakeBB_FBO;

	if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		GetCtxData().m_DrawFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
	else
		GetCtxData().m_ReadFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

	m_Real.glBindFramebuffer(target, framebuffer);
}

bool WrappedOpenGL::Serialise_glDrawBuffer(GLenum buf)
{
	SERIALISE_ELEMENT(GLenum, b, buf);

	if(m_State < WRITING)
	{
		// since we are faking the default framebuffer with our own
		// to see the results, replace back/front/left/right with color attachment 0
		if(b == eGL_BACK_LEFT || b == eGL_BACK_RIGHT || b == eGL_BACK ||
				b == eGL_FRONT_LEFT || b == eGL_FRONT_RIGHT || b == eGL_FRONT)
				b = eGL_COLOR_ATTACHMENT0;

		m_Real.glDrawBuffer(b);
	}

	return true;
}

void WrappedOpenGL::glDrawBuffer(GLenum buf)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFER);
		Serialise_glDrawBuffer(buf);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glDrawBuffer(buf);
}

bool WrappedOpenGL::Serialise_glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)));
	SERIALISE_ELEMENT(uint32_t, num, n);
	SERIALISE_ELEMENT_ARR(GLenum, buffers, bufs, num);

	if(m_State < WRITING)
	{
		for(uint32_t i=0; i < num; i++)
		{
			// since we are faking the default framebuffer with our own
			// to see the results, replace back/front/left/right with color attachment 0
			if(buffers[i] == eGL_BACK_LEFT || buffers[i] == eGL_BACK_RIGHT || buffers[i] == eGL_BACK ||
					buffers[i] == eGL_FRONT_LEFT || buffers[i] == eGL_FRONT_RIGHT || buffers[i] == eGL_FRONT)
					buffers[i] = eGL_COLOR_ATTACHMENT0;
		}

		m_Real.glFramebufferDrawBuffersEXT(GetResourceManager()->GetLiveResource(Id).name, num, buffers);
	}

	delete[] buffers;

	return true;
}

void WrappedOpenGL::glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
	m_Real.glFramebufferDrawBuffersEXT(framebuffer, n, bufs);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE && framebuffer != 0)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);

		ResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
		record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDrawBuffers(GLsizei n, const GLenum *bufs)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		if(GetCtxData().m_DrawFramebufferRecord)
			Serialise_glFramebufferDrawBuffersEXT(GetResourceManager()->GetCurrentResource(GetCtxData().m_DrawFramebufferRecord->GetResourceID()).name, n, bufs);
		else
			Serialise_glFramebufferDrawBuffersEXT(0, n, bufs);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glDrawBuffers(n, bufs);
}

void WrappedOpenGL::glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
	m_Real.glInvalidateFramebuffer(target, numAttachments, attachments);

	if(m_State == WRITING_IDLE)
	{
		GLResourceRecord *record = NULL;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		if(record)
		{
			record->MarkParentsDirty(GetResourceManager());
		}
	}
}

void WrappedOpenGL::glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
	m_Real.glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);

	if(m_State == WRITING_IDLE)
	{
		GLResourceRecord *record = NULL;
		
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(GetCtxData().m_DrawFramebufferRecord) record = GetCtxData().m_DrawFramebufferRecord;
		}
		else
		{
			if(GetCtxData().m_ReadFramebufferRecord) record = GetCtxData().m_ReadFramebufferRecord;
		}

		if(record)
		{
			record->MarkParentsDirty(GetResourceManager());
		}
	}
}

bool WrappedOpenGL::Serialise_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
	SERIALISE_ELEMENT(int32_t, sX0, srcX0);
	SERIALISE_ELEMENT(int32_t, sY0, srcY0);
	SERIALISE_ELEMENT(int32_t, sX1, srcX1);
	SERIALISE_ELEMENT(int32_t, sY1, srcY1);
	SERIALISE_ELEMENT(int32_t, dX0, dstX0);
	SERIALISE_ELEMENT(int32_t, dY0, dstY0);
	SERIALISE_ELEMENT(int32_t, dX1, dstX1);
	SERIALISE_ELEMENT(int32_t, dY1, dstY1);
	SERIALISE_ELEMENT(uint32_t, msk, mask);
	SERIALISE_ELEMENT(GLenum, flt, filter);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glBlitFramebuffer(sX0, sY0, sX1, sY1, dX0, dY0, dX1, dY1, msk, flt);
	}

	return true;
}

void WrappedOpenGL::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BLIT_FRAMEBUFFER);
		Serialise_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void WrappedOpenGL::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
		if(GetResourceManager()->HasCurrentResource(res))
		{
			if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}
	
	m_Real.glDeleteFramebuffers(n, framebuffers);
}

bool WrappedOpenGL::Serialise_glGenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(RenderbufferRes(GetCtx(), *renderbuffers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenRenderbuffers(1, &real);
		m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, real);
		
		GLResource res = RenderbufferRes(GetCtx(), real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);

		m_Textures[live].resource = res;
		m_Textures[live].curType = eGL_RENDERBUFFER;
	}

	return true;
}

void WrappedOpenGL::glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
	m_Real.glGenRenderbuffers(n, renderbuffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_RENDERBUFFERS);
				Serialise_glGenRenderbuffers(1, renderbuffers+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

void WrappedOpenGL::glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
	// don't need to serialise this, as the GL_RENDERBUFFER target does nothing
	// aside from create names (after glGen), and provide as a selector for glRenderbufferStorage*
	// which we do ourselves. We just need to know the current renderbuffer ID
	GetCtxData().m_Renderbuffer = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

	m_Real.glBindRenderbuffer(target, renderbuffer);
}

void WrappedOpenGL::glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
		if(GetResourceManager()->HasCurrentResource(res))
		{
			if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}
	
	m_Real.glDeleteRenderbuffers(n, renderbuffers);
}

bool WrappedOpenGL::Serialise_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(ResourceId, id, GetCtxData().m_Renderbuffer);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		TextureData& texDetails = m_Textures[liveId];

		texDetails.width = Width;
		texDetails.height = Height;
		texDetails.depth = 1;
		texDetails.samples = 1;
		texDetails.curType = eGL_RENDERBUFFER;
		texDetails.internalFormat = Format;

		GLuint real = GetResourceManager()->GetLiveResource(id).name;

		m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, real);
		m_Real.glRenderbufferStorage(eGL_RENDERBUFFER, Format, Width, Height);

		// create read-from texture for displaying this render buffer
		m_Real.glGenTextures(1, &texDetails.renderbufferReadTex);
		m_Real.glBindTexture(eGL_TEXTURE_2D, texDetails.renderbufferReadTex);
		m_Real.glTextureStorage2DEXT(texDetails.renderbufferReadTex, eGL_TEXTURE_2D, 1, Format, Width, Height);

		m_Real.glGenFramebuffers(2, texDetails.renderbufferFBOs);
		m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
		m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
		
		GLenum fmt = GetBaseFormat(Format);

		GLenum attach = eGL_COLOR_ATTACHMENT0;
		if(fmt == eGL_DEPTH_COMPONENT) attach = eGL_DEPTH_ATTACHMENT;
		if(fmt == eGL_STENCIL) attach = eGL_STENCIL_ATTACHMENT;
		if(fmt == eGL_DEPTH_STENCIL) attach = eGL_DEPTH_STENCIL_ATTACHMENT;
		m_Real.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach, eGL_RENDERBUFFER, real);
		m_Real.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach, eGL_TEXTURE_2D, texDetails.renderbufferReadTex, 0);
	}

	return true;
}

void WrappedOpenGL::glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	m_Real.glRenderbufferStorage(target, internalformat, width, height);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(GetCtxData().m_Renderbuffer);
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGE);
		Serialise_glRenderbufferStorage(eGL_RENDERBUFFER, internalformat, width, height);

		record->AddChunk(scope.Get());
	}

	{
		ResourceId rb = GetCtxData().m_Renderbuffer;
		m_Textures[rb].width = width;
		m_Textures[rb].height = height;
		m_Textures[rb].depth = 1;
		m_Textures[rb].samples = 1;
		m_Textures[rb].curType = eGL_RENDERBUFFER;
		m_Textures[rb].dimension = 2;
		m_Textures[rb].internalFormat = internalformat;
	}
}

bool WrappedOpenGL::Serialise_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Samples, samples);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(ResourceId, id, GetCtxData().m_Renderbuffer);

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		TextureData& texDetails = m_Textures[liveId];

		texDetails.width = Width;
		texDetails.height = Height;
		texDetails.depth = 1;
		texDetails.samples = Samples;
		texDetails.curType = eGL_RENDERBUFFER;
		texDetails.internalFormat = Format;
		
		GLuint real = GetResourceManager()->GetLiveResource(id).name;

		m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glRenderbufferStorageMultisample(eGL_RENDERBUFFER, Samples, Format, Width, Height);

		// create read-from texture for displaying this render buffer
		m_Real.glGenTextures(1, &texDetails.renderbufferReadTex);
		m_Real.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex);
		m_Real.glTextureStorage2DMultisampleEXT(texDetails.renderbufferReadTex, eGL_TEXTURE_2D_MULTISAMPLE, Samples, Format, Width, Height, true);

		m_Real.glGenFramebuffers(2, texDetails.renderbufferFBOs);
		m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
		m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
		
		GLenum fmt = GetBaseFormat(Format);

		GLenum attach = eGL_COLOR_ATTACHMENT0;
		if(fmt == eGL_DEPTH_COMPONENT) attach = eGL_DEPTH_ATTACHMENT;
		if(fmt == eGL_STENCIL) attach = eGL_STENCIL_ATTACHMENT;
		if(fmt == eGL_DEPTH_STENCIL) attach = eGL_DEPTH_STENCIL_ATTACHMENT;
		m_Real.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach, eGL_RENDERBUFFER, real);
		m_Real.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach, eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex, 0);
	}

	return true;
}

void WrappedOpenGL::glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	m_Real.glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(GetCtxData().m_Renderbuffer);
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGEMS);
		Serialise_glRenderbufferStorageMultisample(eGL_RENDERBUFFER, samples, internalformat, width, height);

		record->AddChunk(scope.Get());
	}

	{
		ResourceId rb = GetCtxData().m_Renderbuffer;
		m_Textures[rb].width = width;
		m_Textures[rb].height = height;
		m_Textures[rb].depth = 1;
		m_Textures[rb].samples = samples;
		m_Textures[rb].curType = eGL_RENDERBUFFER;
		m_Textures[rb].dimension = 2;
		m_Textures[rb].internalFormat = internalformat;
	}
}
