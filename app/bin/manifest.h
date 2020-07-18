#ifndef HAVE_MANIFEST_H
#define HAVE_MANIFEST_H
	char* CreateManifest(char* nameOfLayout, char* background,
						 char* DependencyDir);
	char* ParseManifest(char* manifest, char* zip_directory);
#endif
