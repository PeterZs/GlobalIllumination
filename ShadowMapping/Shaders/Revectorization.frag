uniform sampler2D discontinuityMap;
uniform sampler2D shadowMap;
uniform sampler2D meshTexturedColor;
varying vec4 shadowCoord;
varying vec3 N;
varying vec3 v;
varying vec3 meshColor;
varying vec2 uvTexture;
uniform mat4 MVP;
uniform mat4 lightMVP;
uniform mat4 inverseLightMVP;
uniform vec3 lightPosition;
uniform int width;
uniform int height;
uniform int shadowMapWidth;
uniform int shadowMapHeight;
uniform int maxSearch;
uniform int useTextureForColoring;
uniform int useMeshColor;

vec4 phong()
{

   vec4 light_ambient = vec4(0.1, 0.1, 0.1, 1);
   vec4 light_specular = vec4(0.1, 0.1, 0.1, 1);
   vec4 light_diffuse = vec4(0.5, 0.5, 0.5, 1);
   float shininess = 60.0;

   vec3 L = normalize(lightPosition.xyz - v);   
   vec3 E = normalize(-v); // we are in Eye Coordinates, so EyePos is (0,0,0)  
   vec3 R = normalize(-reflect(L, N));  
 
   //calculate Ambient Term:  
   vec4 Iamb = light_ambient;    

   //calculate Diffuse Term:  
   vec4 Idiff = light_diffuse * max(dot(N,L), 0.0);    
   
   // calculate Specular Term:
   vec4 Ispec = light_specular * pow(max(dot(R,E),0.0), 0.3 * shininess);

   vec4 sceneColor;
   if(useTextureForColoring == 1)
      sceneColor = texture2D(meshTexturedColor, uvTexture);	
   else if(useMeshColor == 1)
      sceneColor = vec4(meshColor.r, meshColor.g, meshColor.b, 1);
   else
	  sceneColor = gl_FrontLightModelProduct.sceneColor;
   
   return sceneColor + Iamb + Idiff + Ispec;  

}

bool getDisc(vec4 normalizedLightCoord, vec2 dir)
{

	vec2 shadowMapSize = vec2(shadowMapWidth, shadowMapHeight);
	vec2 shadowMapStep = 1.0/shadowMapSize;

	if(normalizedLightCoord.x < 0.0 || normalizedLightCoord.x > 1.0 || normalizedLightCoord.y < 0.0 || normalizedLightCoord.y > 1.0) {
	
		return false;
	
	} else {
	
		float distanceFromLight;
			
		if(dir.x == 0.0) {
		
			normalizedLightCoord.x -= shadowMapStep.x;
			distanceFromLight = texture2D(shadowMap, normalizedLightCoord.st).z;
			float left = (normalizedLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
			bool isLeftUmbra = !bool(left);
			if(isLeftUmbra) return true;

			normalizedLightCoord.x += shadowMapStep.x;
			normalizedLightCoord.x += shadowMapStep.x;
			distanceFromLight = texture2D(shadowMap, normalizedLightCoord.st).z;
			float right = (normalizedLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
			bool isRightUmbra = !bool(right);
			if(isRightUmbra) return true;

		}

		if(dir.y == 0.0) {
	
			normalizedLightCoord.y += shadowMapStep.y;
			distanceFromLight = texture2D(shadowMap, normalizedLightCoord.st).z;
			float bottom = (normalizedLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
			bool isBottomUmbra = !bool(bottom);
			if(isBottomUmbra) return true;

			normalizedLightCoord.y -= shadowMapStep.y;
			normalizedLightCoord.y -= shadowMapStep.y;
			distanceFromLight = texture2D(shadowMap, normalizedLightCoord.st).z;
			float top = (normalizedLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
			bool isTopUmbra = !bool(top);
			if(isTopUmbra) return true;

		}

		return false;
	
	}

	
}

float computeDiscontinuityLength(vec2 inputDiscontinuity, vec4 lightCoord, vec2 dir, int maxSearch)
{

	vec2 imageSize = vec2(width, height);
	vec2 inverseImageSize = 1.0/imageSize;
	vec4 centeredLightCoord = lightCoord;
	
	float foundEdgeEnd = 0.0;
	bool hasDisc = false;
	
	if(dir.x == 0.0 && inputDiscontinuity.r == 0.0) return -1.0;
	if(dir.y == 0.0 && inputDiscontinuity.g == 0.0) return -1.0;
	
	float dist = 1.0;

	vec2 shadowMapStep = dir * inverseImageSize;
	vec2 currentDiscontinuity;
		
	for(int it = 0; it < maxSearch; it++) {
		
		float distanceFromLight = texture2D(shadowMap, centeredLightCoord.st).z;
		float center = (centeredLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
		bool isCenterUmbra = !bool(center);

		if(isCenterUmbra) {
			foundEdgeEnd = 1.0;
			break;
		} else {
		    hasDisc = getDisc(centeredLightCoord, dir);
			if(!hasDisc) break;
		}

		dist++;
		centeredLightCoord.xy += shadowMapStep;
	}
	
	return -dist + foundEdgeEnd * (dist + dist);

}

float normalizeDiscontinuitySpace(vec2 dir, int maxSearch) {
	
	float edgeLength = min(abs(dir.x) + abs(dir.y) - 1.0, float(maxSearch));
	return 1.0 - max(dir.x, dir.y)/edgeLength;
	
}

float orientateDiscontinuitySpace(vec2 dir) 
{

	if(dir.x < 0.0 && dir.y < 0.0)
		return 0.0;
	else if(dir.x > 0.0 && dir.y < 0.0)
		return -0.5;
	else if(dir.x < 0.0 && dir.y > 0.0)
		return 0.5;
	else
		return 1.0;

}

vec4 computeONDS(vec4 lightCoord, vec4 cameraCoord, vec2 discontinuity)
{

	float left = computeDiscontinuityLength(discontinuity, lightCoord, vec2(-1, 0), maxSearch);
	float right = computeDiscontinuityLength(discontinuity, lightCoord, vec2(1, 0), maxSearch);
	float down = computeDiscontinuityLength(discontinuity, lightCoord, vec2(0, -1), maxSearch);
	float up = computeDiscontinuityLength(discontinuity, lightCoord, vec2(0, 1), maxSearch);

	vec2 normalizedDiscontinuityCoord;
	normalizedDiscontinuityCoord.x = normalizeDiscontinuitySpace(vec2(left, right), maxSearch);
	normalizedDiscontinuityCoord.y = normalizeDiscontinuitySpace(vec2(down, up), maxSearch);
	
	vec2 dominantDir;
	dominantDir.x = orientateDiscontinuitySpace(vec2(left, right));
	dominantDir.y = orientateDiscontinuitySpace(vec2(down, up));

	return vec4(normalizedDiscontinuityCoord.x, normalizedDiscontinuityCoord.y, dominantDir.x, dominantDir.y);

}

float computeSubCoord(vec2 cameraCoord, int maxSearch, vec2 dir)
{

	vec2 imageSize = vec2(width, height);
	vec2 subCoord = vec2(0, 0);
	vec2 pos = cameraCoord.xy;
	
	for(int i = 0; i < maxSearch; i++)
	{
		if((texture2D(discontinuityMap, pos).rg * dir) != 0.0)
			subCoord.y++;
		else
			break;
		pos += 1.0/imageSize * dir;
	}

	pos = cameraCoord.xy;
		
	for(int i = 0; i < maxSearch; i++)
	{
		if((texture2D(discontinuityMap, pos).rg * dir) != 0.0)
			subCoord.x++;
		else
			break;
		pos -= 1.0/imageSize * dir;
	}

	pos = cameraCoord.xy;

	return min(subCoord.x/(subCoord.x + subCoord.y - 1.0), float(maxSearch));

}

float clipONDS(vec4 lightCoord, vec4 cameraCoord, vec4 normalizedDiscontinuity, vec2 discontinuity)
{

	vec2 subCoord;

	//pre-evaluation

	if(discontinuity.g > 0.0) {
		
		if(normalizedDiscontinuity.z == 1.0) return 0.0;
		else if(normalizedDiscontinuity.z == 0.0) return 1.0;
		
	}

	if(discontinuity.r > 0.0) {
	
		if(normalizedDiscontinuity.w == 1.0) return 0.0;
		else if(normalizedDiscontinuity.w == 0.0) return 1.0;
		
	}

	if(discontinuity.g > 0.0 && discontinuity.r > 0.0) return 0.0;

	if(discontinuity.g > 0.0) {
		
		
		subCoord.y = computeSubCoord(cameraCoord.xy, maxSearch, vec2(0, 1));

		if(discontinuity.g == 0.5) {

			if((1.0 - subCoord.y) <= normalizedDiscontinuity.x)
				return 0.0;
			else
				return 1.0; 
		
		} else {
		
			if(subCoord.y <= normalizedDiscontinuity.x)
				return 0.0;
			else
				return 1.0;
				 
		}
		
		
	} 
	
	if(discontinuity.r > 0.0) {
		
		subCoord.x = computeSubCoord(cameraCoord.xy, maxSearch, vec2(1, 0));

		if(discontinuity.r == 0.5) {
	
			if(subCoord.x <= normalizedDiscontinuity.y)
				return 0.0;
			else
				return 1.0; 
			
		} else {
		
			if((1.0 - subCoord.x) <= normalizedDiscontinuity.y)
				return 0.0;
			else
				return 1.0;

		}
		
	}
	
}

void main()
{	

	vec4 normalizedLightCoord = shadowCoord / shadowCoord.w;
	vec4 normalizedCameraCoord = vec4(gl_FragCoord.x/float(width), gl_FragCoord.y/float(height), gl_FragCoord.z, 1);
	float distanceFromLight = texture2D(shadowMap, normalizedLightCoord.st).z;		
	float shadow = (normalizedLightCoord.z <= distanceFromLight) ? 1.0 : 0.0; 
	
	if(shadow == 1.0) {

		vec2 discontinuity = texture2D(discontinuityMap, normalizedCameraCoord.xy).rg;
		
		if(discontinuity.r > 0.0 || discontinuity.g > 0.0) {

			vec4 normalizedDiscontinuity = computeONDS(normalizedLightCoord, normalizedCameraCoord, discontinuity);
			float fill = clipONDS(normalizedLightCoord, normalizedCameraCoord, normalizedDiscontinuity, discontinuity);
			gl_FragColor = phong() * shadow * fill;
			//gl_FragColor = vec4(normalizedDiscontinuity.y, 0.0, 0.0, 0.0);
			//gl_FragColor = vec4(1.0 - fill, 0.0, 0.0, 0.0);
			//gl_FragColor = vec4(discontinuity, 0.0, 0.0);

		} else {

			gl_FragColor = phong() * shadow;
			//gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		
		}

	} else {
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
	}
	
}