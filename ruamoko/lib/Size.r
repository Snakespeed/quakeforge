#include "Size.h"

@implementation Size

- (id) initWithComponents: (integer)w : (integer)h
{
	id (self) = [super init];
	width = w;
	height = h;
	return self;
}

- (id) initWithSize: (Size)aSize
{
	id (self) = [super init];

	if (!self || !aSize)
		return NIL;

	width = [aSize width];
	height = [aSize height];

	return self;
}

- (id) copy
{
	local id	myCopy = [super copy];

	if (!myCopy)
		myCopy = [[self class] alloc];

	return [myCopy initWithComponents: width : height];
}

- (id) width
{
	return width;
}

- (id) height
{
	return height;
}

- (void) setSize: (Size)aSize
{
	width = [aSize width];
	height = [aSize height];
}

- (void) setWidth: (integer) w
{
	width = w;
}

- (void) setHeight: (integer) h
{
	height = h;
}

- (void) addSize: (Size)aSize
{
	width += [aSize width];
	height += [aSize height];
}

- (void) subtractSize: (Size)aSize
{
	width += [aSize width];
	height += [aSize height];
}

@end
