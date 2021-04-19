//
// Be aware: VERY UGLY PROTOTYPING CODE!
// This code is NOT correct and not meant to be reused for anything.
// It's a sandbox I used to test out different approaches and I keep it around as a reference.
// 
// NOTE: The pid of the target process is hard coded below.
// 
// NOTE: The program needs priviliged access (sudo) for ptrace() to work. Otherwise the instruction pointer samples will
// always be 0 (at the top left corner).
//

#include <cstdio>
#include <cmath>
#include <cstdint>
using namespace std;

#include <SFML/Graphics.hpp>
using namespace sf;

#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>


struct MemRange {
	uint64_t from, to, offset;
	char* pathname;
	
	uint64_t start_page, page_count;
	Color color;
};

struct ProcessTask {
	pid_t pid;
};

Color random_range_color() {
	union { int r; struct { uint8_t a, r, g, b; } c; } value;
	value.r = rand();
	return Color(value.c.r, value.c.g, value.c.b, 64);
}
/*
Seems to be broken?

//rotate/flip a quadrant appropriately
void rot(int n, int *x, int *y, int rx, int ry) {
    if (ry == 0) {
        if (rx == 1) {
            *x = n-1 - *x;
            *y = n-1 - *y;
        }

        //Swap x and y
        int t  = *x;
        *x = *y;
        *y = t;
    }
}

//convert (x,y) to d
int xy2d(int n, int x, int y) {
    int rx, ry, s, d=0;
    for (s=n/2; s>0; s/=2) {
        rx = (x & s) > 0;
        ry = (y & s) > 0;
        d += s * s * ((3 * rx) ^ ry);
        rot(n, &x, &y, rx, ry);
    }
    return d;
}

//convert d to (x,y)
void d2xy(int n, int d, int *x, int *y) {
    int rx, ry, s, t=d;
    *x = *y = 0;
    for (s=1; s<n; s*=2) {
        rx = 1 & (t/2);
        ry = 1 & (t ^ rx);
        rot(s, x, y, rx, ry);
        *x += s * rx;
        *y += s * ry;
        t /= 4;
    }
}
*/

/**
 * [x, y] -> distance on a hilbert curve
 * @param  {Number}        bits Order of Hilbert curve
 * @param  {Array<Number>} p    2d-Point
 * @return {Number}
 */
uint64_t encode(uint64_t bits, int x, int y) {
  uint64_t d = 0, tmp;
  for (uint64_t s = (1 << bits) / 2; s > 0; s /= 2) {
    uint64_t rx = 0, ry = 0;

    if ((x & s) > 0) rx = 1;
    if ((y & s) > 0) ry = 1;

    d += s * s * ((3 * rx) ^ ry);

    // inlining
    // hilbertRot(s, p, rx, ry)
    if (ry == 0) {
      if (rx == 1) {
        x = s - 1 - x;
        y = s - 1 - y;
      }
      tmp = x;
      x = y;
      y = tmp;
    }
    // end inline
  }
  return d;
}

/**
 * Hilbert curve distance -> [x, y]
 * @param  {Number} bits Order of Hilbert curve
 * @param  {Number} d    Distance
 * @return {Array<Number>} [x, y]
 */
void decode(uint64_t bits, uint64_t d, int* x, int* y) {
  *x = 0; *y = 0;
  uint64_t tmp;
  uint64_t n = 1 << bits;
  for (uint64_t s = 1; s < n; s *= 2) {
    uint64_t rx = 1 & (d / 2);
    uint64_t ry = 1 & (d ^ rx);
    // inlining
    // hilbertRot(s, p, rx, ry)
    if (ry == 0) {
      if (rx == 1) {
        *x = s - 1 - *x;
        *y = s - 1 - *y;
      }
      tmp = *x;
      *x = *y;
      *y = tmp;
    }
    // end inline

    *x += s * rx;
    *y += s * ry;
    d /= 4;
  }
}


float calculate_viewport_scale(RenderWindow& win) {
	float display_size = win.getSize().x;
	float world_size = win.getView().getSize().x;
	// Scale should be: display_size = world_size * scale
	// so: scale = display_size / world_size
	float scale = display_size / world_size;
	return scale;
}
void viewport_move_in_view_space(RenderWindow& win, Vector2i delta) {
	float scale = calculate_viewport_scale(win);
	View view = win.getView();
	view.move(delta.x / scale, delta.y / scale);
	win.setView(view);
}

void viewport_zoom(RenderWindow& win, Vector2i mouse_pos_vs, float factor) {
	float old_scale = calculate_viewport_scale(win);
	// Formula for zooming so that the cursor position stays on the same screen position
	// (we zoom into the point under the cursor):
	// new viewport center ws = mouse pos ws + (old viewport center ws - mouse pos ws) * old viewport scale / new viewport scale
	View view = win.getView();
	Vector2f mouse_pos_ws = win.mapPixelToCoords(mouse_pos_vs);
	float new_scale = old_scale * factor;
	Vector2f old_center = view.getCenter();
	view.setCenter( mouse_pos_ws + (old_center - mouse_pos_ws) * (old_scale / new_scale) );
	view.setSize(Vector2f(win.getSize()) / new_scale);
	win.setView(view);
}


int main() {
	const char* pid = "22768";
	string proc_maps = string("/proc/") + pid + string("/maps");
	
	vector<ProcessTask> tasks;
	DIR *dir;
	struct dirent *ent;
	string proc_tasks = string("/proc/") + pid + string("/task");
	if ((dir = opendir(proc_tasks.c_str())) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] != '.')
				tasks.push_back((ProcessTask){ .pid = atoi(ent->d_name) });
		}
		closedir(dir);
	} else {
		printf("failed to list tasks\n");
	}
	
	/*
	for (auto task : tasks) {
		//printf("%d\n", task.pid);
		
		struct user_regs_struct regs;
		//ptrace(PTRACE_ATTACH, task.pid, NULL, NULL);
		//ptrace(PTRACE_GETREGS, task.pid, NULL, &regs);
		//ptrace(PTRACE_DETACH, task.pid, NULL, NULL);
		
		if ( ptrace(PTRACE_SEIZE, task.pid, NULL, NULL) != 0 )      perror("ptrace(PTRACE_SEIZE)");
		if ( ptrace(PTRACE_INTERRUPT, task.pid, NULL, NULL) != 0 )  perror("ptrace(PTRACE_INTERRUPT)");
		if ( waitpid(task.pid, NULL, 0) != 0 )                      perror("waitpid()");
		if ( ptrace(PTRACE_GETREGS, task.pid, NULL, &regs) != 0 )   perror("ptrace(PTRACE_GETREGS)");
		if ( ptrace(PTRACE_CONT, task.pid, NULL, NULL) != 0 )       perror("ptrace(PTRACE_CONT)");
		
		
		printf("%d rip: 0x%llx\n", task.pid, regs.rip);
		//unsigned long kstkeip = 0;
		//FILE* f = fopen(task.stat_file.c_str(), "rb");
		//fscanf(f, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d %*u %*u %*u %*u %*u %lu", &kstkeip);
		//fclose(f);
		//
		//printf("%s → 0x%016lx\n", task.stat_file.c_str(), kstkeip);
		
	}
	*/
	
	// prepare tasks for profiling
	for (auto task : tasks) {
		if ( ptrace(PTRACE_SEIZE, task.pid, NULL, NULL) != 0 )
			perror("ptrace(PTRACE_SEIZE)");
	}
	
	
	const uint64_t max_gap_size = 1 * 1024*1024;
	const uint64_t page_size = 4096;
	uint64_t prev_end = 0, used_page_count = 0;
	vector<MemRange> ranges;
	
	FILE* f = fopen(proc_maps.c_str(), "rb");
	while ( !feof(f) ) {
		uint64_t from, to, offset;
		char* pathname = NULL;
		int items_read = fscanf(f, "%lx-%lx%*[ ]%*s%*[ ]%lx%*[ ]%*s%*[ ]%*s%*[ ]%m[^\n]", &from, &to, &offset, &pathname);
		if (items_read < 3)
			continue;
		
		uint64_t gap_size = min(from - prev_end, max_gap_size);
		if (gap_size > 0) {
			uint64_t range_pages = gap_size / page_size;
			ranges.push_back((MemRange){ 0, 0, 0, NULL, .start_page = used_page_count, .page_count = range_pages, .color = Color(196, 196, 196) });
			used_page_count += range_pages;
		}
		
		uint64_t range_pages = (to - from) / page_size;
		ranges.push_back((MemRange){ from, to, offset, pathname, .start_page = used_page_count, .page_count = range_pages, .color = random_range_color() });
		used_page_count += range_pages;
		prev_end = to;
	}
	ranges.push_back((MemRange){ 0, 0, 0, NULL, .start_page = used_page_count, .page_count = max_gap_size / page_size, .color = Color(196, 196, 196) });
	fclose(f);
	
	float sqareSideLength = sqrt(used_page_count);
	int hilbertBits = ceil(log2(sqareSideLength));
	sqareSideLength = pow(2, hilbertBits);  // round it up to the next power of 2
	
	Image img;
	img.create((int)sqareSideLength, (int)sqareSideLength);
	for (auto range : ranges) {
		//if (range.pathname)
		//	printf("%s\n", range.pathname);
		for (uint64_t i = 0; i < range.page_count; i++) {
			const int page_index = range.start_page + i;
			int x, y;
			//d2xy(hilbertBits, page_index, &x, &y);
			decode(hilbertBits, page_index, &x, &y);
			img.setPixel(x, y, range.color);
		}
	}
	
	Texture tex;
	tex.loadFromImage(img);
	RectangleShape map(Vector2f(sqareSideLength, sqareSideLength));
	map.setPosition(10, 10);
	map.setTexture(&tex);
	
	
	RenderTexture heatmap;
	heatmap.create(sqareSideLength, sqareSideLength);
	heatmap.clear(Color(0, 0, 0, 0));
	
	
	RenderWindow win(VideoMode(1200, 1000), "Process Map");
	win.setVerticalSyncEnabled(true);
	
	Font font;
	font.loadFromFile("Lato-Regular.ttf");
	
	Clock heatmapDecayClock;
	Vector2i prevMousePos;
	bool mousePanning = false;
	while (win.isOpen()) {
		// Handle events
		Event event;
		while (win.pollEvent(event)) {
			if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Right) {
				prevMousePos = Vector2i(event.mouseButton.x, event.mouseButton.y);
				mousePanning = true;
			} else if (event.type == Event::MouseMoved && mousePanning) {
				Vector2i currentMousePos = Vector2i(event.mouseMove.x, event.mouseMove.y);
				Vector2i delta = currentMousePos - prevMousePos;
				prevMousePos = currentMousePos;
				viewport_move_in_view_space(win, -delta);
			} else if (event.type == Event::MouseButtonReleased && mousePanning) {
				mousePanning = false;
			} else if (event.type == Event::MouseWheelScrolled) {
				float factor = event.mouseWheelScroll.delta < 0 ? 0.9 : 1 / 0.9;
				viewport_zoom(win, Vector2i(event.mouseWheelScroll.x, event.mouseWheelScroll.y), factor);
			} else if (event.type == Event::Closed) {
				win.close();
			}
		}
		
		// Redraw
		win.clear(Color::White);
		
		win.draw(map);
		
		// Profile tasks
		for (auto task : tasks) {
			struct user_regs_struct regs;
			
			if ( ptrace(PTRACE_INTERRUPT, task.pid, NULL, NULL) != 0 )  perror("ptrace(PTRACE_INTERRUPT)");
			//if ( waitpid(task.pid, NULL, 0) != 0 )                      perror("waitpid()");
			waitpid(task.pid, NULL, 0);
			if ( ptrace(PTRACE_GETREGS, task.pid, NULL, &regs) != 0 )   perror("ptrace(PTRACE_GETREGS)");
			if ( ptrace(PTRACE_CONT, task.pid, NULL, NULL) != 0 )       perror("ptrace(PTRACE_CONT)");
			
			uint64_t page_index = 0;
			for (auto range : ranges) {
				if (regs.rip >= range.from && regs.rip < range.to) {
					page_index = range.start_page + (regs.rip - range.from) / page_size;
					break;
				}
			}
			
			int x, y;
			decode(hilbertBits, page_index, &x, &y);
			//printf("%d rip: 0x%llx → pi %llu → %dx%d\n", task.pid, regs.rip, page_index, x, y);
			CircleShape circle;
			circle.setRadius(2);
			circle.setFillColor(Color(0, 0, 255, 168));
			circle.setPosition(map.getTransform().transformPoint(Vector2f(x, y)));
			win.draw(circle);
			
			circle.setRadius(3);
			circle.setFillColor(Color(255, 0, 0, 64));
			circle.setPosition(x, y);
			heatmap.draw(circle);
		}
		
		//if (heatmapDecayClock.getElapsedTime() > seconds(0.5)) {
		//	heatmapDecayClock.restart();
		//	RectangleShape decayRect(Vector2f(sqareSideLength, sqareSideLength));
		//	decayRect.setFillColor(Color(0, 0, 0, 2));
		//	heatmap.draw(decayRect, RenderStates(BlendMode(BlendMode::One, BlendMode::One, BlendMode::ReverseSubtract)));
		//}
		heatmap.display();
		
		Sprite heatmapSprite(heatmap.getTexture());
		heatmapSprite.setPosition(map.getPosition());
		win.draw(heatmapSprite);
		
		
		Vector2f mousePos = win.mapPixelToCoords(Mouse::getPosition(win));
		Vector2f mouseMapPos = map.getInverseTransform().transformPoint(mousePos);
		uint64_t mousePageIndex = encode(hilbertBits, mouseMapPos.x, mouseMapPos.y);
		for (auto range : ranges) {
			if (mousePageIndex >= range.start_page && mousePageIndex < range.start_page + range.page_count) {
				char buffer[512];
				if (range.from == 0 && range.to == 0)
					snprintf(buffer, sizeof(buffer), "empty\n0x%016lx - 0x%016lx\n%.1f MiByte", range.from, range.to, (range.to - range.from) / (float)(1024*1024));
				else
					snprintf(buffer, sizeof(buffer), "%s\n0x%016lx - 0x%016lx offset 0x%lx\n%.1f MiByte", range.pathname, range.from, range.to, range.offset, (range.to - range.from) / (float)(1024*1024));
				
				View worldView = win.getView();
				win.setView(win.getDefaultView());
					Text text;
					text.setPosition(Vector2f(Mouse::getPosition(win)));
					text.setFont(font);
					text.setString(string(buffer));
					text.setCharacterSize(24);
					text.setFillColor(Color(192, 0, 0));
					win.draw(text);
				win.setView(worldView);
				break;
			}
		}
		
		
		Text text;
		text.setPosition(100, 100);
		text.setFont(font);
		text.setString("Hello World!");
		text.setCharacterSize(12);
		text.setFillColor(Color::Red);
		win.draw(text);
		
		win.display();
	}
	
	
	// detatch profiling
	for (auto task : tasks) {
		if ( ptrace(PTRACE_DETACH, task.pid, NULL, NULL) != 0 )
			perror("ptrace(PTRACE_DETACH)");
	}
	
	return 0;
}