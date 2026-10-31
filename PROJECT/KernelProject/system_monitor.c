#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

GtkWidget *cpu_label;
GtkWidget *ram_label;
GtkWidget *process_label;
GtkWidget *uptime_label;

unsigned long long previous_total = 0;
unsigned long long previous_idle = 0;

// Calculate CPU usage using /proc/stat
double get_cpu_usage()
{
    FILE *fp = fopen("/proc/stat", "r");

    if (fp == NULL)
        return 0;

    unsigned long long user, nice, system;
    unsigned long long idle, iowait, irq, softirq, steal;

    int result = fscanf(fp,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user, &nice, &system, &idle,
        &iowait, &irq, &softirq, &steal);

    fclose(fp);

    if (result != 8)
        return 0;

    unsigned long long total =
        user + nice + system + idle +
        iowait + irq + softirq + steal;

    unsigned long long idle_total = idle + iowait;

    double usage = 0;

    if (previous_total != 0)
    {
        unsigned long long total_diff =
            total - previous_total;

        unsigned long long idle_diff =
            idle_total - previous_idle;

        if (total_diff != 0)
        {
            usage = 100.0 *
                (total_diff - idle_diff) / total_diff;
        }
    }

    previous_total = total;
    previous_idle = idle_total;

    return usage;
}

// Read RAM information from /proc/meminfo
void get_ram_usage()
{
    FILE *fp = fopen("/proc/meminfo", "r");

    if (fp == NULL)
        return;

    char line[256];

    unsigned long long total = 0;
    unsigned long long available = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (sscanf(line, "MemTotal: %llu kB", &total) == 1)
            continue;

        if (sscanf(line, "MemAvailable: %llu kB", &available) == 1)
            break;
    }

    fclose(fp);

    unsigned long long used = total - available;

    double percentage = 0;

    if (total != 0)
        percentage = (double)used * 100.0 / total;

    char text[256];

    snprintf(text, sizeof(text),
        "RAM Usage: %.2f%%\n"
        "Used Memory: %llu MB\n"
        "Total Memory: %llu MB",
        percentage,
        used / 1024,
        total / 1024);

    gtk_label_set_text(GTK_LABEL(ram_label), text);
}

// Count running processes using /proc
int get_process_count()
{
    DIR *dir = opendir("/proc");

    if (dir == NULL)
        return 0;

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (isdigit((unsigned char)entry->d_name[0]))
            count++;
    }

    closedir(dir);

    return count;
}

// Read system uptime from /proc/uptime
void get_system_uptime()
{
    FILE *fp = fopen("/proc/uptime", "r");

    if (fp == NULL)
        return;

    double seconds;

    if (fscanf(fp, "%lf", &seconds) != 1)
    {
        fclose(fp);
        return;
    }

    fclose(fp);

    int hours = (int)seconds / 3600;
    int minutes = ((int)seconds % 3600) / 60;
    int secs = (int)seconds % 60;

    char text[100];

    snprintf(text, sizeof(text),
        "System Uptime: %d hours %d minutes %d seconds",
        hours, minutes, secs);

    gtk_label_set_text(GTK_LABEL(uptime_label), text);
}

// Update GUI every second
gboolean update_system_info(gpointer data)
{
    double cpu = get_cpu_usage();

    char cpu_text[100];

    snprintf(cpu_text, sizeof(cpu_text),
        "CPU Usage: %.2f%%", cpu);

    gtk_label_set_text(GTK_LABEL(cpu_label), cpu_text);

    get_ram_usage();

    int processes = get_process_count();

    char process_text[100];

    snprintf(process_text, sizeof(process_text),
        "Running Processes: %d", processes);

    gtk_label_set_text(
        GTK_LABEL(process_label), process_text);

    get_system_uptime();

    return G_SOURCE_CONTINUE;
}

// Create GUI window
static void activate(GtkApplication *app, gpointer data)
{
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *title;

    window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window),
        "Linux System Resource Monitor");

    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);

    gtk_container_set_border_width(
        GTK_CONTAINER(main_box), 25);

    gtk_container_add(GTK_CONTAINER(window), main_box);

    title = gtk_label_new("REAL-TIME SYSTEM MONITOR");

    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *size = pango_attr_size_new(18 * PANGO_SCALE);
    pango_attr_list_insert(attrs, size);
    gtk_label_set_attributes(GTK_LABEL(title), attrs);
    pango_attr_list_unref(attrs);

    gtk_box_pack_start(GTK_BOX(main_box),
        title, FALSE, FALSE, 10);

    cpu_label = gtk_label_new("CPU Usage: Loading...");
    ram_label = gtk_label_new("RAM Usage: Loading...");
    process_label = gtk_label_new("Processes: Loading...");
    uptime_label = gtk_label_new("System Uptime: Loading...");

    gtk_label_set_justify(GTK_LABEL(cpu_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_justify(GTK_LABEL(ram_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_justify(GTK_LABEL(process_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_justify(GTK_LABEL(uptime_label), GTK_JUSTIFY_CENTER);

    gtk_box_pack_start(GTK_BOX(main_box),
        cpu_label, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(main_box),
        ram_label, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(main_box),
        process_label, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(main_box),
        uptime_label, FALSE, FALSE, 10);

    gtk_widget_show_all(window);

    // First update, then refresh every second
    update_system_info(NULL);

    g_timeout_add_seconds(1, update_system_info, NULL);
}

int main(int argc, char **argv)
{
    GtkApplication *app;

    app = gtk_application_new(
        "com.kernel.systemmonitor",
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate",
        G_CALLBACK(activate), NULL);

    int status = g_application_run(
        G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}
