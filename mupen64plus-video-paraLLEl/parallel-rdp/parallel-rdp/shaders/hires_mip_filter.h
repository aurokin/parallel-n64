#ifndef HIRES_MIP_FILTER_H_
#define HIRES_MIP_FILTER_H_

// Mip samples contain straight RGB after alpha-weighted bilinear filtering.
// Weight them by opacity again when crossing mip levels so transparent
// samples cannot introduce a dark fringe. Scalar arithmetic keeps this
// production helper usable by both GLSL and the display-free C++ test.
vec4 hires_alpha_weighted_mix(vec4 a, vec4 b, float fraction)
{
	float weight_a = (1.0f - fraction) * a.w;
	float weight_b = fraction * b.w;
	float alpha = weight_a + weight_b;
	if (alpha == 0.0f)
		return vec4(0.0f, 0.0f, 0.0f, 0.0f);

	return vec4((a.x * weight_a + b.x * weight_b) / alpha,
	            (a.y * weight_a + b.y * weight_b) / alpha,
	            (a.z * weight_a + b.z * weight_b) / alpha,
	            alpha);
}

#endif
