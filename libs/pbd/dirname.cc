#include <cstdio>
#include <cstdlib>
#include <string>
#include <pbd/dirname.h>


char *
PBD::dirname (const char *path)

{
	char *slash;
	size_t len;
	char *ret;
	
	if ((slash = strrchr (path, '/')) == 0) {
		return strdup (path);
	}
	
	if (*(slash+1) == '\0') {
		return strdup ("");
	}

	len = (size_t) (slash - path);
	ret = (char *) malloc (sizeof (char) * (len + 1));

	snprintf (ret, len, "%*s", len, path);
	return ret;
}

std::string 
PBD::dirname (const std::string str)
{
	std::string::size_type slash = str.find_last_of ('/');
	
	if (slash == std::string::npos) {
		return str;
	}
	return str.substr (0, slash);

}
