#include "battery.h"
#include <leif/color.h>
#include <leif/layout.h>
#include <leif/util.h>
#include <leif/task.h>
#include <sys/inotify.h>

#include "config.h"
#include <leif/ez_api.h>
#include <leif/widget.h>
#include <pthread.h>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

extern state_t s;

typedef struct {
  lf_ui_state_t* ui;
} task_data_t;

#define EVENT_SIZE    (sizeof(struct inotify_event))
#define BUF_LEN       (1024 * (EVENT_SIZE + NAME_MAX + 1))

static pthread_mutex_t battery_mutex = PTHREAD_MUTEX_INITIALIZER;

static void btrywidget(lf_ui_state_t* ui);
static void refreshbtrys(void);

int32_t readbtry(const char *name);



static void rerender_battery_task(void* data);

static void rerender_battery_task_refresh(void* data);

void rerender_battery_task(void* data) {
  lf_ui_state_t* ui = ((task_data_t*)data)->ui;
  lf_component_rerender(ui, btrywidget);
  free(data);
}

void rerender_battery_task_refresh(void* data) {
  task_data_t* d = (task_data_t*)data;
  refreshbtrys(); 
  lf_component_rerender(d->ui, btrywidget);
  free(d);
}

int readbtry(const char *name) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", name);
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  int percent = -1;
  fscanf(f, "%d", &percent);
  fclose(f);
  return percent;
}

void replacefirst(char *out, size_t out_size,
                   const char *input,
                   const char *target,
                   const char *replacement) {
  // Find the first occurrence of the target substring
  const char *pos = strstr(input, target);
  if (!pos) {
    // If not found, just copy input to output
    snprintf(out, out_size, "%s", input);
    return;
  }

  // Calculate the lengths of parts
  size_t prefix_len = pos - input;
  size_t target_len = strlen(target);
  size_t replacement_len = strlen(replacement);
  size_t suffix_len = strlen(pos + target_len);

  // Check if total output will fit
  size_t needed_len = prefix_len + replacement_len + suffix_len + 1;
  if (needed_len > out_size) {
    fprintf(stderr, "boron: warning: output buffer for replacement too small (%zu needed, %zu available)\n",
            needed_len, out_size);
  }

  // Perform the replacement
  snprintf(out, out_size, "%.*s%s%s",
           (int)prefix_len,
           input,
           replacement,
           pos + target_len);
}

void 
refreshbtrys(void) {
  DIR *dir = opendir("/sys/class/power_supply");
  if (!dir) return;

  s.nbatteries = 0;
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL && s.nbatteries < MAX_BATTERIES) {
    char type_path[PATH_MAX];
    snprintf(
      type_path, 
      sizeof(type_path), 
      "/sys/class/power_supply/%s/type", entry->d_name);

    FILE *type = fopen(type_path, "r");
    if (!type) continue;

    char type_str[64];
    fgets(type_str, sizeof(type_str), type);
    fclose(type);
    type_str[strcspn(type_str, "\n")] = 0;

    if (strcmp(type_str, "Battery") == 0) {
      strncpy(
        s.batteries[s.nbatteries].name, entry->d_name, MAX_NAME_LEN - 1);
      s.batteries[s.nbatteries].last_percent = readbtry(entry->d_name);
      s.nbatteries++;
    }
  }

  closedir(dir);
}

void*
btrylisten(void *arg) {
  while (true) {
    bool changed = false;
    for (int i = 0; i < s.nbatteries; ++i) {
      int current = readbtry(s.batteries[i].name);
      if (current != s.batteries[i].last_percent && current != -1) {
        s.batteries[i].last_percent = current;
        changed = true;
      }
    }


      if (s.battery_widget && changed) {
      task_data_t* task_data = malloc(sizeof(task_data_t));
      task_data->ui = s.battery_widget->ui;
      lf_task_enqueue(rerender_battery_task, task_data);
    }
    usleep(1 * 1000000);
  }

  return NULL;
}

void* btrywatch(void *arg) {
  state_t* s = (state_t*)arg;
  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    perror("inotify_init");
    return NULL;
  }

  int wd = inotify_add_watch(fd, "/sys/class/power_supply", IN_CREATE | IN_DELETE);
  if (wd < 0) {
    perror("watch /sys/class/power_supply");
    close(fd);
    return NULL;
  }

  char buffer[BUF_LEN];

  while (1) {
    int length = read(fd, buffer, BUF_LEN);
    if (length > 0) {
      // Background thread does nothing with shared state
      task_data_t* task_data = malloc(sizeof(task_data_t));
      task_data->ui = s->battery_widget->ui;
      lf_task_enqueue(rerender_battery_task, task_data);
    }
    usleep(200000);
  }

  close(fd);
  return NULL;
}


void 
btrywidget(lf_ui_state_t* ui) {
  lf_div(ui)->base.scrollable = false;
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 10); 
  lf_style_widget_prop_color(
    ui, lf_crnt(ui), border_color, lf_color_from_hex(0xcccccc)); 
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2); 
    lf_widget_set_padding(ui, lf_crnt(ui), 20.0f);
  lf_style_widget_prop_color(
    ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));
  lf_text_h1(ui, "Battery");
  lf_widget_set_font_style(ui, lf_crnt(ui), 
                           LF_FONT_STYLE_BOLD);

  for(uint32_t i = 0; i < s.nbatteries; i++) {
    lf_div(ui);
    lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
    char buf_name[32];
    char* icon = "";
    int32_t percent = s.batteries[i].last_percent;
    if(percent >= 75) 
      icon = "";
    else if(percent >= 50)
      icon = "";
    else if (percent >= 25)
      icon = "";
    else if(percent >= 5)
      icon = "";
    else 
      icon = "";
    char display_name[64];
    replacefirst(display_name, sizeof(display_name), s.batteries[i].name, "BAT", "Laptop ");
    sprintf(buf_name, "%s  %s", icon, display_name); 
    lf_text_h3(ui, buf_name);
    lf_widget_set_font_style(ui, lf_crnt(ui), 
                           LF_FONT_STYLE_BOLD);

    char buf[32];
    sprintf(buf, "%i", s.batteries[i].last_percent);

    lf_grower(ui);

    lf_text_h3(ui, buf);
    lf_style_widget_prop(ui, lf_crnt(ui), margin_right, -2.5);
    lf_widget_set_font_style(ui, lf_crnt(ui), 
                           LF_FONT_STYLE_BOLD);
    lf_style_widget_prop(ui, lf_crnt(ui), margin_left, 0);
    lf_text_h3(ui, "%");
  lf_div_end(ui);
  }

  if(!s.nbatteries) {
    lf_text_h4(ui, "󱉝 No batteries deteced");
      lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_dim(lf_color_from_hex(barcolorforeground), 80));
    }
  lf_div_end(ui);

}

bool btrysetup(void) {
  pthread_mutex_lock(&battery_mutex);
  refreshbtrys();
  pthread_mutex_unlock(&battery_mutex);
  
  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, btrylisten, (void *)&s) != 0) {
    fprintf(stderr, "boron: battery: error creating listener thread\n");
    return false;
  }
  pthread_t watchthread;
  if (pthread_create(&watchthread, NULL, btrywatch, (void *)&s) != 0) {
    fprintf(stderr, "boron: battery: error creating watcher thread\n");
    return false;
  }
  return true; 
}

bool 
btrycreatewidget(lf_window_t barwin) {
  if(!s.pvstate)
    s.pvstate = pv_init();
  if(!s.pvstate) return false;
  s.battery_widget = pv_widget(
    s.pvstate, "boron_battery_popup", btrywidget,
    s.bararea.x + s.bararea.width - 300 - 50, 
    s.bararea.y + s.bararea.height + 10,
    300, 170);


  pv_widget_set_popup_of(s.pvstate, s.battery_widget, barwin);
  lf_widget_set_font_family(s.battery_widget->ui, s.battery_widget->ui->root, barfont);
  lf_widget_set_font_style(s.battery_widget->ui, s.battery_widget->ui->root, LF_FONT_STYLE_REGULAR);

  pv_widget_hide(s.battery_widget);
  pv_widget_set_animation(s.battery_widget, PV_WIDGET_ANIMATION_SLIDE_OUT_VERT, 0.2, lf_ease_out_cubic);

  return true;

}
