/*
yamltojson.c

Copyright 2020 Bumblebee Laboratories Pte Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the Software), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <yaml.h>
#include <stdarg.h>
#include <json-glib/json-glib.h>

enum
{
  JNONE,
  JOBJECT,
  JARRAY,
  JVALUE,
  LESSLESS
};

GQueue *stack;
GHashTable *alias;

char *
get_event_name (int id)
{
  switch (id)
    {
    case YAML_NO_EVENT:
      return "YAML_NO_EVENT";
    case YAML_STREAM_START_EVENT:
      return "YAML_STREAM_START_EVENT";
    case YAML_STREAM_END_EVENT:
      return "YAML_STREAM_END_EVENT";
    case YAML_DOCUMENT_START_EVENT:
      return "YAML_DOCUMENT_START_EVENT";
    case YAML_DOCUMENT_END_EVENT:
      return "YAML_DOCUMENT_END_EVENT";
    case YAML_ALIAS_EVENT:
      return "YAML_ALIAS_EVENT";
    case YAML_SCALAR_EVENT:
      return "YAML_SCALAR_EVENT";
    case YAML_SEQUENCE_START_EVENT:
      return "YAML_SEQUENCE_START_EVENT";
    case YAML_SEQUENCE_END_EVENT:
      return "YAML_SEQUENCE_END_EVENT";
    case YAML_MAPPING_START_EVENT:
      return "YAML_MAPPING_START_EVENT";
    case YAML_MAPPING_END_EVENT:
      return "YAML_MAPPING_END_EVENT";
    default:
      return "unknown event";
    }
}

int
printe (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  return vfprintf (stderr, fmt, args);
}

void
push (int n)
{
  int *p = g_malloc (sizeof (int));
  *p = n;
  g_queue_push_tail (stack, p);
  //g_print ("pushed %d\n", n);
}

int
pop ()
{
  int *p = g_queue_pop_tail (stack);
  int n = *p;
  g_free (p);
  //g_print ("popped %d\n", n);
  return n;
}

void
anchor (gchar * key, JsonNode * n)
{
  if (key != NULL)
    {
      gchar *keyc = g_strdup (key);
      g_hash_table_insert (alias, keyc, n);
      //g_print ("anchor %s\n", key);
      //g_print ("json node type %d\n", JSON_NODE_TYPE (n));
      if (n == NULL)
	{
	  g_print ("null alias\n");
	  exit (1);
	}
    }
}

JsonNode *
get_alias (gchar * key)
{
  JsonNode *n = g_hash_table_lookup (alias, key);
  //g_print ("get alias %s returns node type %d\n", key, JSON_NODE_TYPE (n));
  if (n == NULL)
    {
      g_print ("null alias\n");
      exit (1);
    }
  return n;
}

void
checkobject (JsonNode * n)
{
  JsonObject *p = json_node_get_object (n);
  if (p == NULL)
    {
      g_print ("no object\n");
      exit (1);
    }
}

void
dup_member (JsonObject * object, const gchar * member_name,
	    JsonNode * member_node, gpointer user_data)
{
  //g_print("dupping %s\n",member_name);
  JsonNode *dm = json_object_dup_member (object, member_name);
  JsonObject *dst = user_data;
  json_object_set_member (dst, member_name, dm);
}

void
set_node (JsonNode * dst, JsonNode * src)
{
  GValue v = G_VALUE_INIT;
  JsonObject *m;
  JsonArray *a;
  const gchar *vs;
  switch (JSON_NODE_TYPE (src))
    {
    case JSON_NODE_VALUE:
      json_node_get_value (src, &v);
      vs = g_value_get_string (&v);
      if (vs == NULL)
	{
	  g_print ("null string\n");
	  exit (1);
	}
      json_node_init_string (dst, vs);
      g_value_unset (&v);
      break;
    case JSON_NODE_OBJECT:
      //checkobject (src);
      m = json_node_dup_object (src);
      json_node_init_object (dst, m);
      //checkobject (dst);
      break;
    case JSON_NODE_ARRAY:
      a = json_node_dup_array (src);
      json_node_init_array (dst, a);
      break;
    default:
      g_print ("cannot set node for type %d\n", JSON_NODE_TYPE (src));
      exit (1);
      break;
    }
}

int
main ()
{
  JsonNode *n = json_node_alloc ();
  JsonNode *r;
  JsonObject *m;
  JsonObject *mc;
  JsonArray *a;
  int nstate = JNONE;
  yaml_parser_t parser;
  yaml_event_t event;
  int done = 0;
  stack = g_queue_new ();
  alias = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  yaml_parser_initialize (&parser);
  yaml_parser_set_input_file (&parser, stdin);
  int ee = 1;

  while (!done)
    {
      if (!yaml_parser_parse (&parser, &event))
	goto error;

      //printf ("event %s start %d end %d\n", get_event_name (event.type), event.start_mark, event.end_mark);

      switch (event.type)
	{
	case YAML_ALIAS_EVENT:
	  //printf ("alias %s\n", event.data.alias.anchor);
	  r = get_alias (event.data.alias.anchor);
	  switch (nstate)
	    {
	    case JVALUE:
	      set_node (n, r);
	      r = json_node_get_parent (n);
	      //g_print ("json alias parent ptr %p\n", r);
	      if (r == NULL)
		{
		  //g_print ("no parent\n");
		}
	      else
		{
		  n = r;
		  nstate = pop ();
		}
	      break;
	    case LESSLESS:
	      //checkobject (r);
	      mc = json_node_get_object (r);
	      m = json_node_get_object (n);
	      json_object_foreach_member (mc, dup_member, m);
	      //checkobject (n);
	      nstate = pop ();
	      break;
	    default:
	      g_print ("unknown json nstate %d\n", nstate);
	      exit (1);
	      break;
	    }
	  break;
	case YAML_SCALAR_EVENT:
	  //printf ("scalar anchor %s tag %s value %s\n", event.data.scalar.anchor, event.data.scalar.tag, event.data.scalar.value);
	  //g_print ("current nstate %d\n", nstate);
	  switch (nstate)
	    {
	    case JOBJECT:
	      if (g_strcmp0 (event.data.scalar.value, "<<") == 0)
		{
		  push (nstate);
		  nstate = LESSLESS;
		}
	      else
		{
		  //checkobject (n);
		  m = json_node_get_object (n);
		  //g_print ("json object ptr %p\n", m);
		  r = json_node_alloc ();
		  json_node_set_parent (r, n);
		  json_object_set_member (m, event.data.scalar.value, r);
		  n = r;
		  push (nstate);
		  nstate = JVALUE;
		}
	      break;
	    case JARRAY:
	      a = json_node_get_array (n);
	      //g_print ("json array ptr %p\n", a);
	      r = json_node_alloc ();
	      json_node_set_parent (r, n);
	      json_node_init_string (r, event.data.scalar.value);
	      json_array_add_element (a, r);
	      break;
	    case JVALUE:
	      json_node_init_string (n, event.data.scalar.value);
	      anchor (event.data.sequence_start.anchor, n);
	      r = json_node_get_parent (n);
	      //g_print ("json value parent ptr %p\n", r);
	      if (r == NULL)
		{
		  //g_print ("no parent\n");
		}
	      else
		{
		  n = r;
		  nstate = pop ();
		}
	      break;
	    default:
	      g_print ("unknown json nstate %d\n", nstate);
	      exit (1);
	      break;
	    }
	  break;
	case YAML_SEQUENCE_START_EVENT:
	  //printf ("sequence anchor %s tag %s\n", event.data.sequence_start.anchor, event.data.sequence_start.tag);
	  a = json_array_new ();
	  json_node_init_array (n, a);
	  anchor (event.data.sequence_start.anchor, n);
	  push (nstate);
	  nstate = JARRAY;
	  break;
	case YAML_MAPPING_START_EVENT:
	  //printf ("mapping anchor %s tag %s\n", event.data.mapping_start.anchor, event.data.mapping_start.tag);
	  switch (nstate)
	    {
	    case JNONE:
	    case JVALUE:
	      m = json_object_new ();
	      json_node_init_object (n, m);
	      //checkobject (n);
	      anchor (event.data.mapping_start.anchor, n);
	      push (nstate);
	      nstate = JOBJECT;
	      break;
	    case JARRAY:
	      a = json_node_get_array (n);
	      //g_print ("json array ptr %p\n", a);
	      r = json_node_alloc ();
	      json_node_set_parent (r, n);
	      json_array_add_element (a, r);
	      m = json_object_new ();
	      json_node_init_object (r, m);
	      //checkobject (r);
	      n = r;
	      anchor (event.data.mapping_start.anchor, n);
	      push (nstate);
	      nstate = JOBJECT;
	      break;
	    default:
	      g_print ("unknown json node type %d\n", nstate);
	      exit (1);
	      break;
	    }
	  break;
	case YAML_SEQUENCE_END_EVENT:
	  r = json_node_get_parent (n);
	  if (r == NULL)
	    {
	      //g_print ("no parent\n");
	    }
	  else
	    {
	      n = r;
	      do
		{
		  nstate = pop ();
		}
	      while (nstate == 3);
	    }
	  break;
	case YAML_MAPPING_END_EVENT:
	  r = json_node_get_parent (n);
	  if (r == NULL)
	    {
	      //g_print ("no parent\n");
	    }
	  else
	    {
	      n = r;
	      do
		{
		  nstate = pop ();
		}
	      while (nstate == 3);
	    }
	  break;
	}
      done = (event.type == YAML_STREAM_END_EVENT);

      yaml_event_delete (&event);
    }

  ee = 0;

error:
  yaml_parser_delete (&parser);

  JsonGenerator *jg = json_generator_new ();
  json_generator_set_root (jg, n);
  json_generator_set_pretty (jg, TRUE);

  char *d = json_generator_to_data (jg, NULL);

  g_print ("%s\n", d);
  g_free (d);

  g_queue_free_full (stack, g_free);
  g_hash_table_unref (alias);

  g_object_unref (jg);

  return ee;
}
