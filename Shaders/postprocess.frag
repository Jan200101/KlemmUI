#version 330

layout(location = 0) out vec4 f_color;
in vec2 v_texcoords;
uniform sampler2D u_ui;
uniform bool u_hasWindowBorder = false;
uniform vec3 u_borderColor = vec3(1);

vec3 sampleUI()
{
	return texture(u_ui, v_texcoords).xyz;
}
void main()
{
	f_color.xyz = sampleUI();
	f_color.w = 1;

	if (u_hasWindowBorder)
	{
		vec2 EdgeSize = vec2(2.0) / textureSize(u_ui, 0);
		if (v_texcoords.x <= EdgeSize.x)
		{
			f_color = vec4(u_borderColor, 1);
		}
		if (v_texcoords.y <= EdgeSize.y)
		{
			f_color = vec4(u_borderColor, 1);
		}
		if (1.0 - v_texcoords.x <= EdgeSize.x)
		{
			f_color = vec4(u_borderColor, 1);
		}
		if (1.0 - v_texcoords.y <= EdgeSize.y)
		{
			f_color = vec4(u_borderColor, 1);
		}
	}
}