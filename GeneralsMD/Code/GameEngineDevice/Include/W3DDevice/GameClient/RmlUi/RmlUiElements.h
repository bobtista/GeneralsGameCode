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

// Ronin @feature 28/10/2026 DX9: the game's own elements for RmlUi documents. Ported from tintinhamans' GeneralsGameCode fork
// (arctic/go/feat/rmlui); his <scrolllog> comes when a screen needs it.
#pragma once

#include <RmlUi/Core/CallbackTexture.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>

class MapMetaData;
class VideoBuffer;
struct IDirect3DTexture9;

// <gametext key="GUI:Label"/>: the localised string, from TheGameText.
class RmlGameTextElement : public Rml::Element
{
public:
	explicit RmlGameTextElement(const Rml::String &tag) : Rml::Element(tag) {}

protected:
	virtual void OnAttributeChange(const Rml::ElementAttributes &changed_attributes) override;
	virtual void OnUpdate() override;

private:
	bool m_dirty = false;	// set by the key; the text is built on the next update, after the parser is done
};

// Ronin @feature 29/10/2026 DX9: <mappedimage name="Foo"/>: an INI MappedImage as an <img> cropped to its rect on the atlas
// page (the shell screens' art). His, from the same fork.
class RmlMappedImageElement : public Rml::Element
{
public:
	explicit RmlMappedImageElement(const Rml::String &tag) : Rml::Element(tag) {}

protected:
	virtual void OnAttributeChange(const Rml::ElementAttributes &changed_attributes) override;
	virtual void OnUpdate() override;

private:
	void refresh();

	bool m_dirty = false;		// rebuilt on update, after the parser has added any children
	Rml::String m_shownName;	// a data binding sets the name again every update; rebuild only when it changes
};

// Ronin @feature 30/10/2026 DX9: tintinhamans' <mappreview> (arctic/go/feat/rmlui), as he wrote it: the map's preview
// art, its supply and tech sites, fitted to the map's own shape.
class RmlMapPreviewElement : public Rml::Element
{
public:
	explicit RmlMapPreviewElement(const Rml::String &tag);
	virtual ~RmlMapPreviewElement() override;

protected:
	virtual void OnAttributeChange(const Rml::ElementAttributes &changed_attributes) override;
	virtual void OnUpdate() override;

private:
	void refresh();
	void fitToAspect(float aspect);
	void refreshSites(const MapMetaData *md);

	// Rebuilt on update, after the parser has added any children (e.g. a <select> copying an option).
	bool m_dirty = false;
};

// Ronin @feature 29/10/2026 DX9: <video source="Name" fit="fill|contain|cover"/>: a movie the game decodes into a VideoBuffer
// (the portrait movie a script plays), drawn from that buffer's texture; nothing while no buffer is published by that name.
class RmlVideoElement : public Rml::Element
{
public:
	explicit RmlVideoElement(const Rml::String &tag) : Rml::Element(tag) {}
	virtual ~RmlVideoElement() override;

	// null clears it; an owner clears or replaces its buffer before deleting it
	static void setSource(const Rml::String &name, VideoBuffer *buffer);

protected:
	virtual void OnRender() override;
	virtual void OnResize() override;
	virtual void OnAttributeChange(const Rml::ElementAttributes &changed_attributes) override;
	virtual void OnPropertyChange(const Rml::PropertyIdSet &changed_properties) override;

private:
	void bind(VideoBuffer *buffer);
	void buildGeometry();

	VideoBuffer *m_buffer = nullptr;			// only compared once bound
	IDirect3DTexture9 *m_d3dTexture = nullptr;	// our own reference
	Rml::Vector2i m_textureSize;
	Rml::Vector2f m_videoSize;
	Rml::CallbackTexture m_texture;
	Rml::Geometry m_geometry;
	bool m_geometryDirty = true;
};

// Ronin @feature 31/10/2026 DX9: tintinhamans' <scrolllog> (the score and disconnect screens' chat): as a line comes in it
// jumps to the new bottom, unless scrolled up to read; checked against the last layout's height, so a frame after the line.
class RmlScrollLogElement : public Rml::Element
{
public:
	explicit RmlScrollLogElement(const Rml::String &tag) : Rml::Element(tag) {}

protected:
	virtual void OnUpdate() override
	{
		Rml::Element::OnUpdate();
		const float height = GetScrollHeight();
		if (height != m_lastHeight)
		{
			m_lastHeight = height;
			if (m_following)
				SetScrollTop(height);
		}
		else
			m_following = GetScrollTop() + GetClientHeight() >= height - 1.0f;
	}

private:
	float m_lastHeight = 0.0f;
	bool m_following = true;	// false once scrolled away from the bottom
};
