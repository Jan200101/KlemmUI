#define _CRT_SECURE_NO_WARNINGS
#include "TextRenderer.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../Util/stb_truetype.h"
#include "../Shader.h"
#include <SDL.h>
#include <vector>
#include <iostream>
#include <Application.h>
#include <Rendering/ScrollObject.h>
#include <StringReplace.h>
#include <GL/glew.h>

namespace _TextRenderer
{
	static Shader* TextShader = nullptr;
	std::vector<TextRenderer*> Renderers;
}

//void TextRenderer::OnWindowResized()
//{
//	_TextRenderer::projection = glm::ortho(-450 * Graphics::AspectRatio, 450 * Graphics::AspectRatio, 450.0f, -450.f);
//}

void TextRenderer::CheckForTextShader()
{
	if (!_TextRenderer::TextShader)
	{
		_TextRenderer::TextShader = new Shader(Application::GetShaderPath() + "/text.vert", Application::GetShaderPath() + "/text.frag");
	}
}


size_t TextRenderer::GetCharacterIndexADistance(ColoredText Text, float Dist, float Scale, Vector2f& LetterOutLocation)
{
	float originalScale = Scale;
	Scale /= CharacterSizeInPixels;
	Scale *= 60.0f;
	stbtt_bakedchar* cdata = (stbtt_bakedchar*)cdatapointer;
	std::string TextString = TextSegment::CombineToString(Text);
	TextString.append(" ");
	float MaxHeight = 0.0f;
	float x = 0.f, y = 0.f;
	Uint32 numVertices = 0;
	size_t i = 0;
	float PrevDepth = 0;
	float PrevMaxDepth = 0;
	for (auto& c : TextString)
	{
		bool IsTab = false;
		if (c == '	')
		{
			c = ' ';
			IsTab = true;
		}
		if (c >= 32 && c < 128)
		{
			stbtt_aligned_quad q;
			for (int i = 0; i < (IsTab ? 4 : 1); i++)
			{
				stbtt_GetBakedQuad(cdata, 2048, 2048, c - 32, &x, &y, &q, 1);
			}
			MaxHeight = std::max(q.y1 - q.y0, MaxHeight);
			numVertices += 6;
			if (q.x0 / 1800 / Application::AspectRatio * Scale > Dist)
			{
				//std::cout << q.x0 / 225 / Application::AspectRatio;

				LetterOutLocation = Vector2f(PrevDepth / 1800 / Application::AspectRatio, 0) * Scale;
				if (i && q.x0 / 1800 / Application::AspectRatio * Scale > Dist + 0.0075) return i - 1;
				return i;
			}
			PrevMaxDepth = q.x1;
			PrevDepth = q.x0;
		}
		i++;
	}

	LetterOutLocation = Vector2f(PrevMaxDepth / 1800 / Application::AspectRatio, 0) * Scale;
	return TextString.size() - 1;
}

TextRenderer::TextRenderer(std::string filename, float CharacterSizeInPixels)
{
	CheckForTextShader();
	_TextRenderer::Renderers.push_back(this);
	stbtt_bakedchar* cdata = new stbtt_bakedchar[96];
	Uint8* ttfBuffer = (Uint8*)malloc(1 << 20);
	Uint8* tmpBitmap = new Uint8[2048 * 2048];
	if (ttfBuffer == NULL)
	{
		throw "EPIC MALLOC FAILURE";
	}
	this->CharacterSizeInPixels = CharacterSizeInPixels;
	Filename = filename;

	fread(ttfBuffer, 1, 1 << 20, fopen(Filename.c_str(), "rb"));
	stbtt_BakeFontBitmap(ttfBuffer, 0, CharacterSizeInPixels, tmpBitmap, 2048, 2048, 32, 96, cdata); // no guarantee this fits!
	// can free ttf_buffer at this point
	glGenTextures(1, &fontTexture);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 2048, 2048, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tmpBitmap);
	// can free temp_bitmap at this point
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glGenVertexArrays(1, &fontVao);
	glBindVertexArray(fontVao);
	glGenBuffers(1, &fontVertexBufferId);
	glBindBuffer(GL_ARRAY_BUFFER, fontVertexBufferId);
	fontVertexBufferCapacity = 35;
	fontVertexBufferData = new FontVertex[fontVertexBufferCapacity * 6];

	glBufferData(GL_ARRAY_BUFFER, sizeof(FontVertex) * 6 * fontVertexBufferCapacity, 0, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, texCoords));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, color));
	glBindVertexArray(0);

	cdatapointer = cdata;
	free(ttfBuffer);
	delete[] tmpBitmap;
}

Vector2f TextRenderer::GetTextSize(ColoredText Text, float Scale, bool Wrapped, float LengthBeforeWrap)
{
	LengthBeforeWrap *= 1350 * Application::AspectRatio;
	float originalScale = Scale;
	Scale /= CharacterSizeInPixels;
	Scale *= 7.5f;
	stbtt_bakedchar* cdata = (stbtt_bakedchar*)cdatapointer;
	float MaxHeight = 0.0f;
	float x = 0.f, y = 0.f;
	float MaxX = 0.0f;
	FontVertex* vData = fontVertexBufferData;
	Uint32 numVertices = 0;
	size_t Wraps = 0;
	for (auto& seg : Text)
	{
		size_t LastWordIndex = SIZE_MAX;
		size_t LastWrapIndex = 0;
		size_t LastWordNumVertices = 0;
		FontVertex* LastWordVDataPtr = nullptr;
		for (size_t i = 0; i < seg.Text.size(); i++)
		{
			bool IsTab = seg.Text[i] == '	';
			if (IsTab)
			{
				seg.Text[i] = ' ';
			}
			if (seg.Text[i] >= 32 && seg.Text[i] < 128)
			{
				stbtt_aligned_quad q;
				for (int txIt = 0; txIt < (IsTab ? 4 : 1); txIt++)
				{
					stbtt_GetBakedQuad(cdata, 2048, 2048, seg.Text[i] - 32, &x, &y, &q, 1);
				}
				if (seg.Text[i] == ' ')
				{
					LastWordIndex = i;
					LastWordNumVertices = numVertices;
					LastWordVDataPtr = vData;
				}

				MaxHeight = std::max(q.y1 - q.y0, MaxHeight);
				vData += 6;
				numVertices += 6;
				MaxX = std::max(MaxX, x);
				if (x > LengthBeforeWrap / CharacterSizeInPixels * 150 && Wrapped)
				{
					Wraps++;
					if (LastWordIndex != SIZE_MAX && LastWordIndex != LastWrapIndex)
					{
						i = LastWordIndex;
						LastWrapIndex = i;
						vData = LastWordVDataPtr;
						numVertices = LastWordNumVertices;
					}
					x = 0;
					y += CharacterSizeInPixels;
				}
			}
		}
	}
	return (Vector2f(MaxX, y + CharacterSizeInPixels) / Vector2f(1350 * Application::AspectRatio, 1350)) * Scale * 6;
}


DrawableText* TextRenderer::MakeText(ColoredText Text, Vector2f Pos, float Scale, Vector3f32 Color, float opacity, float LengthBeforeWrap)
{
	for (auto& i : Text)
	{
		StrReplace::ReplaceChar(i.Text, '	', "    ");
	}
	GLuint newVAO = 0, newVBO = 0;
	glGenVertexArrays(1, &newVAO);
	glBindVertexArray(newVAO);
	glGenBuffers(1, &newVBO);
	glBindBuffer(GL_ARRAY_BUFFER, newVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(FontVertex) * 6 * fontVertexBufferCapacity, 0, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, texCoords));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, color));
	glBindVertexArray(0);

	LengthBeforeWrap *= 1350 * Application::AspectRatio;
	float originalScale = Scale;
	Scale /= CharacterSizeInPixels;
	Scale *= 7.5f;
	Pos.X = Pos.X * 450 * Application::AspectRatio;
	Pos.Y = Pos.Y * -450;
	stbtt_bakedchar* cdata = (stbtt_bakedchar*)cdatapointer;
	glBindVertexArray(newVAO);
	glBindBuffer(GL_ARRAY_BUFFER, newVBO);
	size_t len = TextSegment::CombineToString(Text).size();
	if (fontVertexBufferCapacity < len) {
		fontVertexBufferCapacity = len;
		glBufferData(GL_ARRAY_BUFFER, sizeof(FontVertex) * 6 * fontVertexBufferCapacity, 0, GL_DYNAMIC_DRAW);
		delete[] fontVertexBufferData;
		fontVertexBufferData = new FontVertex[fontVertexBufferCapacity * 6];
	}
	float MaxHeight = 0.0f;
	float x = 0.f, y = 0.f;
	FontVertex* vData = fontVertexBufferData;
	Uint32 numVertices = 0;
	for (auto& seg : Text)
	{
		size_t LastWordIndex = SIZE_MAX;
		size_t LastWrapIndex = 0;
		size_t LastWordNumVertices = 0;
		FontVertex* LastWordVDataPtr = nullptr;
		for (size_t i = 0; i < seg.Text.size(); i++)
		{
			if (seg.Text[i] >= 32 && seg.Text[i] < 128)
			{
				stbtt_aligned_quad q;
				stbtt_GetBakedQuad(cdata, 2048, 2048, seg.Text[i] - 32, &x, &y, &q, 1);
				vData[0].position = Vector2(q.x0, q.y1); vData[0].texCoords = Vector2(q.s0, q.t1);
				vData[1].position = Vector2(q.x1, q.y1); vData[1].texCoords = Vector2(q.s1, q.t1);
				vData[2].position = Vector2(q.x1, q.y0); vData[2].texCoords = Vector2(q.s1, q.t0);
				vData[3].position = Vector2(q.x0, q.y0); vData[3].texCoords = Vector2(q.s0, q.t0);
				vData[4].position = Vector2(q.x0, q.y1); vData[4].texCoords = Vector2(q.s0, q.t1);
				vData[5].position = Vector2(q.x1, q.y0); vData[5].texCoords = Vector2(q.s1, q.t0);

				vData[0].color = seg.Color;		vData[1].color = seg.Color;
				vData[2].color = seg.Color;		vData[3].color = seg.Color;
				vData[4].color = seg.Color;		vData[5].color = seg.Color;

				if (seg.Text[i] == ' ')
				{
					LastWordIndex = i;
					LastWordNumVertices = numVertices;
					LastWordVDataPtr = vData;
				}

				MaxHeight = std::max(q.y1 - q.y0, MaxHeight);
				vData += 6;
				numVertices += 6;
				if (x > LengthBeforeWrap / CharacterSizeInPixels * 150)
				{
					if (LastWordIndex != SIZE_MAX && LastWordIndex != LastWrapIndex)
					{
						i = LastWordIndex;
						LastWrapIndex = i;
						vData = LastWordVDataPtr;
						numVertices = LastWordNumVertices;
					}
					x = 0;
					y += CharacterSizeInPixels;
				}
			}
		}
	}
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(FontVertex) * numVertices, fontVertexBufferData);
	return new DrawableText(newVAO, newVBO, numVertices, fontTexture, Pos, Scale, Color, opacity);
}

void TextRenderer::Reinit()
{
	if (fontVertexBufferData)
	{
		delete[] fontVertexBufferData;
	}
	delete[] (stbtt_bakedchar*)cdatapointer;
	stbtt_bakedchar* cdata = new stbtt_bakedchar[96];
	Uint8* ttfBuffer = (Uint8*)malloc(1 << 20);
	Uint8* tmpBitmap = new Uint8[2048 * 2048];
	if (ttfBuffer == NULL)
	{
		throw "EPIC MALLOC FAILURE";
	}
	fread(ttfBuffer, 1, 1 << 20, fopen(Filename.c_str(), "rb"));
	stbtt_BakeFontBitmap(ttfBuffer, 0, CharacterSizeInPixels, tmpBitmap, 2048, 2048, 32, 96, cdata); // no guarantee this fits!
	// can free ttf_buffer at this point
	glGenTextures(1, &fontTexture);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 2048, 2048, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tmpBitmap);
	// can free temp_bitmap at this point
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glGenVertexArrays(1, &fontVao);
	glBindVertexArray(fontVao);
	glGenBuffers(1, &fontVertexBufferId);
	glBindBuffer(GL_ARRAY_BUFFER, fontVertexBufferId);

	glBufferData(GL_ARRAY_BUFFER, sizeof(FontVertex) * 6 * fontVertexBufferCapacity, 0, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, texCoords));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void*)offsetof(FontVertex, color));
	glBindVertexArray(0);

	cdatapointer = cdata;
	free(ttfBuffer);
	delete[] tmpBitmap;
}

TextRenderer::~TextRenderer()
{
	unsigned int i = 0;
	for (TextRenderer* r : _TextRenderer::Renderers)
	{
		if (r == this)
			_TextRenderer::Renderers.erase(_TextRenderer::Renderers.begin() + i);
		i++;
	}
	glDeleteTextures(1, &fontTexture);
	glDeleteBuffers(1, &fontVertexBufferId);
	glDeleteBuffers(1, &fontVao);
	if (fontVertexBufferData)
	{
		delete[] fontVertexBufferData;
	}
	delete[] (stbtt_bakedchar*)cdatapointer;
}

void OnWindowResized()
{
}

DrawableText::DrawableText(unsigned int VAO, unsigned int VBO, unsigned int NumVerts, unsigned int Texture,
	Vector2f Position, float Scale, Vector3f32 Color, float opacity)
{
	this->Position = Position;
	this->Scale = Scale;
	this->NumVerts = NumVerts;
	this->VAO = VAO;
	this->VBO = VBO;
	this->Opacity = opacity;
	this->Texture = Texture;
	this->Color = Color;
}

void DrawableText::Draw(ScrollObject* CurrentScrollObject)
{
	TextRenderer::CheckForTextShader();
	glBindVertexArray(VAO);
	_TextRenderer::TextShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Texture);
	glUniform1i(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "u_texture"), 0);
	glUniform3f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "textColor"), Color.X, Color.Y, Color.Z);
	glUniform1f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "u_aspectratio"), Application::AspectRatio);
	glUniform3f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "transform"), (float)Position.X, (float)Position.Y, Scale);
	glUniform1f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "u_opacity"), Opacity);
	if (CurrentScrollObject != nullptr)
	{
		glUniform3f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "u_offset"),
			-CurrentScrollObject->Percentage, CurrentScrollObject->Position.Y, CurrentScrollObject->Position.Y - CurrentScrollObject->Scale.Y);
	}
	else
		glUniform3f(glGetUniformLocation(_TextRenderer::TextShader->GetShaderID(), "u_offset"), 0, -1000, 1000);
	glDrawArrays(GL_TRIANGLES, 0, NumVerts);
}

DrawableText::~DrawableText()
{
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
}
