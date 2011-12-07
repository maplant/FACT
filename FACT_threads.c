/* This file is part of Furlow VM.
 *
 * FACT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FACT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FACT. If not, see <http://www.gnu.org/licenses/>.
 */

#include <FACT.h>

void
FACT_send_message (FACT_num_t msg, size_t dest) /* Add a message to a thread's queue. */
{
  FACT_thread_t curr;
  struct FACT_thread_queue *last;

  /* Find the thread number "dest". If it doesn't exist or is dead,
   * throw an error.
   */
  for (curr = threads; curr != NULL; curr = curr->next)
    {
      if (curr->thread_num == dest)
	goto found_thread;
    }
  /* The thread does not exist, throw an error. */
  FACT_throw_error (CURR_THIS, "no thread number %zu exists", dest);

 found_thread: /* We've found the thread. */
  if (curr->run_flag == T_DEAD) /* The thread has terminated, throw an error. */
    FACT_throw_error (CURR_THIS, "thread number %zu is dead", dest);

  /* Gain control of the message queue and add the message. */
  pthread_mutex_lock (&curr->queue_lock);

  if (curr->root_message == NULL)
    {
      /* Add the root message. */
      curr->root_message = FACT_malloc (sizeof (struct FACT_thread_queue));
      curr->root_message->sender_id = curr_thread->thread_num;
      curr->root_message->msg = FACT_alloc_num ();
      FACT_set_num (curr->root_message->msg, msg);
    }
  else
    {
      /* Find the next open spot in the linked list. */
      for (last = curr->root_message; last->next != NULL; last = last->next);
      last->next = FACT_malloc (sizeof (struct FACT_thread_queue));
      last = last->next;
      last->sender_id = curr_thread->thread_num;
      last->msg = FACT_alloc_num ();
      FACT_set_num (last->msg, msg);
    }

  if (curr->num_messages == 0) /* Unblock the thread, if we must. */
    pthread_cond_signal (&curr->msg_block);
  
  curr->num_messages++;
  pthread_mutex_unlock (&curr->queue_lock);
}

FACT_scope_t
FACT_get_next_message (void) /* Pop the current thread's message queue. */
{
  FACT_num_t sender, message;
  FACT_scope_t msg_holder;
  
  pthread_mutex_lock (&curr_thread->queue_lock);
  
  /* If there are no messages, block. */
  if (curr_thread->num_messages == 0)
    pthread_cond_wait (&curr_thread->msg_block, &curr_thread->queue_lock);

  /* Create a scope to represent the message. */
  msg_holder = FACT_alloc_scope ();
  sender = FACT_add_num (msg_holder, "sender");
  message = FACT_add_num (msg_holder, "message");

  mpc_set_ui (sender->value, curr_thread->root_message->sender_id);
  FACT_set_num (message, curr_thread->root_message->msg);

  /* Remove the element from the queue and return. */
  curr_thread->root_message = curr_thread->root_message->next;
  curr_thread->num_messages--;
  pthread_mutex_unlock (&curr_thread->queue_lock);
  return msg_holder;
}

