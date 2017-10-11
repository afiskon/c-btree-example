// (c) Aleksandr A Alekseev 2008 | http://eax.me/
// gcc btree.c -o btree

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NODES_COUNT 4

char tree_file[] = "tree.dat";
char data_file[] = "data.dat";

typedef unsigned long ulong;

typedef struct {
  char name[256];
  char department[256];
  char phone[32];
} node_value;

typedef struct {
  ulong id;
  ulong offset;
  union {
    ulong left_offset;
    ulong right_offset;
  };
} node;

typedef struct {
  ulong nodes_count;
  ulong right_offset;
  node nodes[MAX_NODES_COUNT];
} exnode;

FILE* ftree;
FILE* fdata;
node new_node;
long root_offset;

long write_exnode(exnode* ex)
{
  long t;
  fseek(ftree, 0, SEEK_END);
  t = ftell(ftree);
  fwrite((char*)ex, sizeof(exnode), 1, ftree);
  return t;
}

void print_data(long offset)
{
  node_value nodev;
  fseek(fdata, offset, SEEK_SET);
  fread((char*)&nodev, sizeof(node_value), 1, fdata);

  printf("Name: %s\nDepartment: %s\nPhone: %s\n", nodev.name, nodev.department, nodev.phone);
}

void ftree_init() {
  exnode new_root;
  root_offset = sizeof(long);
  new_root.nodes_count = 0;
  memset((char*)&new_root, 0, sizeof(exnode));
  fwrite((char*)&root_offset, sizeof(long), 1, ftree);
  fwrite((char*)&new_root, sizeof(exnode), 1, ftree);
}

void tree_new_root() {
  exnode new_root;
  memset((char*)&new_root, 0, sizeof(exnode));
  new_root.nodes_count = 1;
  new_root.nodes[0].id = new_node.id;
  new_root.nodes[0].offset = new_node.offset;
  new_root.nodes[0].left_offset = root_offset;
  new_root.right_offset = new_node.right_offset;

  root_offset = write_exnode(&new_root);
}

long prewrite_data() {
  fseek(fdata, 0, SEEK_END);
  return ftell(fdata);
}

void write_data(node_value* nodev) {
  fwrite((char*)nodev, sizeof(node_value), 1, fdata);
}

void insert_node_simple(exnode* ex, ulong pos)
{
  ulong* change;
  ulong move_size = ex->nodes_count - pos;

  if(move_size) {
    move_size *= sizeof(node);
    memmove(&(ex->nodes[pos+1]), &(ex->nodes[pos]), move_size);
  }

  memcpy(&(ex->nodes[pos]), &new_node, sizeof(node));

  if(new_node.right_offset) {
    change = move_size ? &(ex->nodes[pos+1].left_offset) : &(ex->right_offset);
    ex->nodes[pos].left_offset = *change;
    *change = new_node.right_offset;
  }

  ex->nodes_count++;
}

void insert_node_ex(exnode* ex, ulong pos)
{
  exnode newex;
  ulong t = (pos > MAX_NODES_COUNT / 2);
  ulong copy_size = (MAX_NODES_COUNT / 2)*sizeof(node);
  ulong copy_from = MAX_NODES_COUNT / 2;

  if(t) {
    pos -= (MAX_NODES_COUNT / 2 + 1);
    copy_size -= sizeof(node);
    copy_from++;
  }

  memcpy(&(newex.nodes), &(ex->nodes[copy_from]), copy_size);
  ex->nodes_count = copy_from;
  newex.nodes_count = MAX_NODES_COUNT - copy_from;
  newex.right_offset = ex->right_offset;
  ex->right_offset = newex.nodes[0].left_offset;

  insert_node_simple(t ? &newex : ex, pos);

  memcpy(&new_node, &(ex->nodes[MAX_NODES_COUNT/2]), sizeof(node));
  newex.nodes[0].left_offset = ex->right_offset;
  ex->right_offset = new_node.left_offset;
  new_node.right_offset = write_exnode(&newex);
  ex->nodes_count--;
}

int insert_node(exnode* ex, ulong pos)
{
  int t = (ex->nodes_count == MAX_NODES_COUNT);
  if(t == 1) insert_node_ex(ex, pos);
  else insert_node_simple(ex, pos);
  return t;
}

void tree_search(ulong id)
{
  exnode ex;
  ulong i;
  long offset = root_offset;

  while(offset)
  {
    fseek(ftree, offset, SEEK_SET);
    fread((char*)&ex, sizeof(exnode), 1, ftree);
    offset = 0;

    for(i = 0; i < ex.nodes_count; i++)
    {
      if(ex.nodes[i].id == id)
      {
        print_data(ex.nodes[i].offset);
        return;
      }

      if(ex.nodes[i].id > id)
      {
        offset = ex.nodes[i].left_offset;
        break;
      }
    }

    if(!offset) offset = ex.right_offset;
  }

  printf("Nothing found!\n");
}

int tree_add_rec(long offset)
{
  exnode ex;
  ulong i;
  int t = 1;
  ulong overwrite = 0;
  ulong failed = 1;

  fseek(ftree, offset, SEEK_SET);
  fread((char*)&ex, sizeof(exnode), 1, ftree);

  for(i = 0; i < ex.nodes_count; i++) {
    if(ex.nodes[i].id == new_node.id) {
      printf("Employee already exists!\n");
      return -1;
    }

    if(new_node.id < ex.nodes[i].id) {
      if(ex.nodes[i].left_offset) t = tree_add_rec(ex.nodes[i].left_offset);
      if(t == 1){ overwrite = 1; t = insert_node(&ex, i); }

      failed--;
      break;
    }
  }

  if(failed) {
    if(ex.right_offset) t = tree_add_rec(ex.right_offset);
    if(t == 1){ overwrite = 1; t = insert_node(&ex, ex.nodes_count); }
  }

  if(overwrite) {
    fseek(ftree, offset, SEEK_SET);
    fwrite((char*)&ex, sizeof(exnode), 1, ftree);
  }
  return t;
}

int tree_add(ulong id, ulong data_offset) {
  int t;
  new_node.id = id;
  new_node.offset = data_offset;
  new_node.right_offset = 0;

  t = tree_add_rec(root_offset);
  if(t == 1) tree_new_root();

  return t;
}

char* get_string(char* buff, ulong buff_size)
{
  char* t = fgets(buff, buff_size, stdin);
  if(t) { *(buff + strlen(buff) - 1) = '\x00'; }
  return t;
}

void init()
{
  long t;
  fseek(ftree, 0, SEEK_END);
  t = ftell(ftree);
  fseek(ftree, 0, SEEK_SET);
  if(t) fread((char*)&root_offset, sizeof(long), 1, ftree);
  else ftree_init();
}

int main(int argc, char* argv[]) {
  char buff[16];
  node_value nodev;

  ftree = fopen(tree_file, "r+");
  if(!ftree) ftree = fopen(tree_file, "w+");
  if(!ftree) { printf("Cann't open %s!\n", tree_file); return 0; }

  fdata = fopen(data_file, "r+");
  if(!fdata) fdata = fopen(data_file, "w+");
  if(!fdata) { printf("Cann't open %s!\n", data_file); fclose(ftree); return 0; }

  init();

  printf("Enter command:\n");
  while(get_string(buff, sizeof(buff)))
  {
    if(strcmp(buff, "quit") == 0)
    {
      printf("Goodbie!\n");
      break;
    }
    else if(strcmp(buff, "add") == 0)
    {
      printf("ID: "); if(!fgets(buff, sizeof(buff), stdin)) break;
      printf("Name: "); if(!get_string(nodev.name, sizeof(nodev.name))) break;
      printf("Department: "); if(!get_string(nodev.department, sizeof(nodev.department))) break;
      printf("Phone: "); if(!get_string(nodev.phone, sizeof(nodev.phone))) break;

      if(tree_add(atoi(buff), prewrite_data()) >= 0) {
        write_data(&(nodev));
        printf("Done!\n");
      }
    }
    else if(strcmp(buff, "search") == 0)
    {
      printf("Input ID: ");
      fgets(buff, sizeof(buff), stdin);
      tree_search(atoi(buff));
    }
    else if(strcmp(buff, "help") == 0) printf("add    - add employee to the database\n"
      "search - search for employee by id\nquit   - leave programm\nhelp   - this message\n");
    else printf("Not understood!\n");
  }

  fseek(ftree, 0, SEEK_SET);
  fwrite((char*)&root_offset, sizeof(long), 1, ftree);
  fclose(ftree);
  fclose(fdata);
  return 0;
}
