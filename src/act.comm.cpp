/* ************************************************************************
*   File: act.comm.c                                    Part of CircleMUD *
*  Usage: Player-level communication commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iostream>

using namespace std;

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "screen.h"
#include "awake.h"
#include "newmatrix.h"
#include "newdb.h"
#include "constants.h"
#include "string_safety.h"

/* extern variables */
extern struct skill_data skills[];

extern void respond(struct char_data *ch, struct char_data *mob, char *str);
extern bool can_send_act_to_target(struct char_data *ch, bool hide_invisible, struct obj_data * obj, void *vict_obj, struct char_data *to, int type);
extern char *how_good(int skill, int percent);
int find_skill_num(char *name);


ACMD_DECLARE(do_say);

ACMD_CONST(do_say) {
  static char not_const[MAX_STRING_LENGTH];
  STRCPY(not_const, argument);
  do_say(ch, not_const, cmd, subcmd);
}

ACMD(do_say)
{
  int success, suc;
  struct char_data *tmp, *to = NULL;
  
  skip_spaces(&argument);
  if (!*argument)
    send_to_char(ch, "Yes, but WHAT do you want to say?\r\n");
  else if (subcmd != SCMD_OSAY && !PLR_FLAGGED(ch, PLR_MATRIX) && !char_can_make_noise(ch))
    send_to_char("You can't seem to make any noise.\r\n", ch);
  else {
    if (AFF_FLAGGED(ch, AFF_RIG)) {
      send_to_char(ch, "You have no mouth.\r\n");
      return;
    } else if (PLR_FLAGGED(ch, PLR_MATRIX)) {
      if (ch->persona) {
        // Send the self-referencing message to the decker and store it in their history.
        snprintf(buf, sizeof(buf), "You say, \"%s^n\"\r\n", capitalize(argument));
        send_to_icon(ch->persona, buf);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
        
        // Send the message to the rest of the host. Store it to the recipients' says history.
        snprintf(buf, sizeof(buf), "%s says, \"%s^n\"\r\n", ch->persona->name, capitalize(argument));
        // send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
        for (struct matrix_icon *i = matrix[ch->persona->in_host].icons; i; i = i->next_in_host) {
          if (ch->persona != i && i->decker && has_spotted(i, ch->persona)) {
            send_to_icon(i, buf);
            if (i->decker && i->decker->ch)
              store_message_to_history(i->decker->ch->desc, COMM_CHANNEL_SAYS, buf);
          }
        }
      } else {
        for (struct char_data *targ = get_ch_in_room(ch)->people; targ; targ = targ->next_in_room)
          if (targ != ch && PLR_FLAGGED(targ, PLR_MATRIX)) {
            // Send and store.
            snprintf(buf, sizeof(buf), "Your hitcher says, \"%s^n\"\r\n", capitalize(argument));
            send_to_char(buf, targ);
            store_message_to_history(targ->desc, COMM_CHANNEL_SAYS, buf);
          }
        // Send and store.
        snprintf(buf, sizeof(buf), "You send, down the line, \"%s^n\"\r\n", capitalize(argument));
        send_to_char(buf, ch);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
      }
      return;
    }
    if (subcmd == SCMD_SAYTO) {
      half_chop(argument, buf, buf2);
      STRCPY(argument, buf2);
      if (ch->in_veh)
        to = get_char_veh(ch, buf, ch->in_veh);
      else
        to = get_char_room_vis(ch, buf);
    }
    if (ch->in_veh) {
      if(subcmd == SCMD_OSAY) {
        snprintf(buf, sizeof(buf), "%s says ^mOOCly^n, \"%s^n\"\r\n",GET_NAME(ch), capitalize(argument));
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_OSAYS, perform_act(buf, ch, NULL, NULL, tmp));
          }
        }
      } else {
        success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh)
          if (tmp != ch) {
            if (to) {
              if (to == tmp)
                snprintf(buf2, MAX_STRING_LENGTH, " to you");
              else
                snprintf(buf2, MAX_STRING_LENGTH, " to %s", CAN_SEE(tmp, to) ? (found_mem(GET_MEMORY(tmp), to) ? CAP(found_mem(GET_MEMORY(tmp), to)->mem)
                        : GET_NAME(to)) : "someone");
            }
            if (success > 0) {
              suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                snprintf(buf, sizeof(buf), "$z says%s in %s, \"%s^n\"",
                        (to ? buf2 : ""), skills[GET_LANGUAGE(ch)].name, capitalize(argument));
              else
                snprintf(buf, sizeof(buf), "$z speaks%s in a language you don't understand.", (to ? buf2 : ""));
            } else
              snprintf(buf, sizeof(buf), "$z mumbles incoherently.");
            if (IS_NPC(ch))
              snprintf(buf, sizeof(buf), "$z says%s, \"%s^n\"", (to ? buf2 : ""), capitalize(argument));
            // Note: includes act()
            store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
          }
      }
    } else {
      /** new code by WASHU **/
      if(subcmd == SCMD_OSAY) {
        snprintf(buf, sizeof(buf), "$z ^nsays ^mOOCly^n, \"%s^n\"", capitalize(argument));
        for (tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people); tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_OSAYS, perform_act(buf, ch, NULL, NULL, tmp));
          }
        }
      } else {
        success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
        for (tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people); tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room))
          if (tmp != ch && !(IS_ASTRAL(ch) && !CAN_SEE(tmp, ch))) {
            if (to) {
              if (to == tmp)
                snprintf(buf2, MAX_STRING_LENGTH, " to you");
              else
                snprintf(buf2, MAX_STRING_LENGTH, " to %s", CAN_SEE(tmp, to) ? (found_mem(GET_MEMORY(tmp), to) ? CAP(found_mem(GET_MEMORY(tmp), to)->mem)
                        : GET_NAME(to)) : "someone");
            }
            if (success > 0) {
              suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                snprintf(buf, sizeof(buf), "$z says%s in %s, \"%s^n\"",
                        (to ? buf2 : ""), skills[GET_LANGUAGE(ch)].name, capitalize(argument));
              else
                snprintf(buf, sizeof(buf), "$z speaks%s in a language you don't understand.", (to ? buf2 : ""));
            } else
              snprintf(buf, sizeof(buf), "$z mumbles incoherently.");
            if (IS_NPC(ch))
              snprintf(buf, sizeof(buf), "$z says%s, \"%s^n\"", (to ? buf2 : ""), capitalize(argument));
            // Invokes act().
            store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
          }
      }
    }
    // Acknowledge transmission of message.
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      delete_doubledollar(argument);
      if(subcmd == SCMD_OSAY) {
        snprintf(buf, sizeof(buf), "You say ^mOOCly^n, \"%s^n\"\r\n", capitalize(argument));
        send_to_char(buf, ch);
        store_message_to_history(ch->desc, COMM_CHANNEL_OSAYS, buf);
      } else {
        if (to)
          snprintf(buf2, MAX_STRING_LENGTH, " to %s", CAN_SEE(ch, to) ? (found_mem(GET_MEMORY(ch), to) ?
                                                     CAP(found_mem(GET_MEMORY(ch), to)->mem) : GET_NAME(to)) : "someone");
        snprintf(buf, sizeof(buf), "You say%s, \"%s^n\"\r\n", (to ? buf2 : ""), capitalize(argument));
        send_to_char(buf, ch);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
      }
    }
  }
}

ACMD(do_exclaim)
{
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Yes, but WHAT do you like to exclaim?\r\n");
    return;
  }
  
  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  snprintf(buf, sizeof(buf), "$z ^nexclaims, \"%s!^n\"", capitalize(argument));
  if (ch->in_veh) {
    for (struct char_data *tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp));
      }
    }
  } else {
    for (struct char_data *tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people); tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp));
      }
    }
  }
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    snprintf(buf, sizeof(buf), "You exclaim, \"%s!^n\"\r\n", capitalize(argument));
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
  }
}

void perform_tell(struct char_data *ch, struct char_data *vict, char *arg)
{
  snprintf(buf, sizeof(buf), "^r$n tells you, '%s^r'^n", capitalize(arg));
  store_message_to_history(vict->desc, COMM_CHANNEL_TELLS, act(buf, FALSE, ch, 0, vict, TO_VICT | TO_SLEEP));

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
  {
    snprintf(buf, sizeof(buf), "^rYou tell $N, '%s'^n", capitalize(arg));
    store_message_to_history(ch->desc, COMM_CHANNEL_TELLS, act(buf, FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP));
  }

  if (!IS_NPC(ch))
    GET_LAST_TELL(vict) = GET_IDNUM(ch);
  else
    GET_LAST_TELL(vict) = NOBODY;
}

ACMD(do_tell)
{
  struct char_data *vict = NULL;
  SPECIAL(johnson);

  half_chop(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Who do you wish to tell what??\r\n", ch);
    return;
  }
  if (!(vict = get_player_vis(ch, buf, 0))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    return;
  }
  if (!IS_NPC(vict) && !vict->desc)      /* linkless */
    act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR);
  if (!IS_SENATOR(ch) && !IS_SENATOR(vict)) {
    send_to_char("Tell is for communicating with immortals only.\r\n", ch);
    return;
  }

  if (PRF_FLAGGED(vict, PRF_NOTELL))
    act("$E can't hear you.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PLR_FLAGGED(vict, PLR_WRITING) ||
           PLR_FLAGGED(vict, PLR_MAILING))
    act("$E's writing a message right now; try again later.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PRF_FLAGGED(vict, PRF_AFK))
    act("$E's AFK at the moment.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PLR_FLAGGED(vict, PLR_EDITING))
    act("$E's editing right now, try again later.", FALSE, ch, 0, vict, TO_CHAR);
  else
    perform_tell(ch, vict, buf2);
}

ACMD(do_reply)
{
  struct char_data *tch = character_list;

  skip_spaces(&argument);

  if (GET_LAST_TELL(ch) == NOBODY)
    send_to_char("You have no-one to reply to!\r\n", ch);
  else if (!*argument)
    send_to_char("What is your reply?\r\n", ch);
  else {
    /* Make sure the person you're replying to is still playing by searching
     * for them.  Note, this will break in a big way if I ever implement some
     * scheme where it keeps a pool of char_data structures for reuse.
     */

    for (; tch != NULL; tch = tch->next)
      if (!IS_NPC(tch) && GET_IDNUM(tch) == GET_LAST_TELL(ch))
        break;

    if (tch == NULL || (tch && GET_IDNUM(tch) != GET_LAST_TELL(ch)))
      send_to_char("They are no longer playing.\r\n", ch);
    else
      perform_tell(ch, tch, argument);
  }
}

ACMD(do_ask)
{
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Yes, but WHAT do you like to ask?\r\n");
    return;
  }
  
  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  snprintf(buf, sizeof(buf), "$z asks, \"%s?^n\"", capitalize(argument));
  if (ch->in_veh) {
    for (struct char_data *tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp));
      }
    }
  } else {
    for (struct char_data *tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people); tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp));
      }
    }
  }
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    // TODO
    snprintf(buf, sizeof(buf), "You ask, \"%s?^n\"\r\n", capitalize(argument));
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
  }
}

ACMD(do_spec_comm)
{
  struct veh_data *veh = NULL;
  struct char_data *vict;
  const char *action_sing, *action_plur, *action_others;
  int success, suc;
  if (subcmd == SCMD_WHISPER) {
    action_sing = "whisper to";
    action_plur = "whispers to";
    action_others = "$z whispers something to $N.";
  } else {
    action_sing = "ask";
    action_plur = "asks";
    action_others = "$z asks $N something.";
  }

  half_chop(argument, buf, buf2);
  success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  if (!*buf || !*buf2) {
    send_to_char(ch, "Whom do you want to %s... and what??\r\n", action_sing);
  } else if (ch->in_veh) {
    if (ch->in_veh->cspeed > SPEED_IDLE) {
      send_to_char("You're going to hit your head on a lamppost if you try that.\r\n", ch);
      return;
    }
    ch->in_room = ch->in_veh->in_room;
    struct veh_data *last_veh = ch->in_veh;
    ch->in_veh = ch->in_veh->in_veh;
    if ((vict = get_char_room_vis(ch, buf))) {
      if (vict == ch) {
        send_to_char(ch, "Why would you want to %s yourself?\r\n", action_sing);
        return;
      }
      if (success > 0) {
        snprintf(buf, sizeof(buf), "You lean out towards $N and say, \"%s\"", capitalize(buf2));
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_CHAR));
        suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
        if (suc > 0)
          snprintf(buf, sizeof(buf), "From within %s^n, $z says to you in %s, \"%s^n\"\r\n",
                  GET_VEH_NAME(last_veh), skills[GET_LANGUAGE(ch)].name, capitalize(buf2));
        else
          snprintf(buf, sizeof(buf), "From within %s^n, $z speaks in a language you don't understand.\r\n", GET_VEH_NAME(last_veh));
      } else
        snprintf(buf, sizeof(buf), "$z mumbles incoherently from %s.\r\n", GET_VEH_NAME(last_veh));
      store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_VICT));
    } else {
      send_to_char("You don't see them here.\r\n", ch);
    }
    ch->in_room = NULL;
    ch->in_veh = last_veh;
  } else if (!(vict = get_char_room_vis(ch, buf)) &&
             !((veh = get_veh_list(buf, ch->in_room->vehicles, ch)) && subcmd == SCMD_WHISPER))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", capitalize(buf));
  else if (vict == ch)
    send_to_char("You can't get your mouth close enough to your ear...\r\n", ch);
  else if (IS_ASTRAL(ch) && vict &&
           !(IS_ASTRAL(vict) ||
             PLR_FLAGGED(vict, PLR_PERCEIVE) ||
             IS_DUAL(vict)))
    send_to_char("That is most assuredly not possible.\r\n", ch);
  else {
    if (veh) {
      if (veh->cspeed > SPEED_IDLE) {
        send_to_char(ch, "It's moving too fast for you to lean in.\r\n");
        return;
      }

      snprintf(buf, sizeof(buf), "You lean into a %s and say, \"%s\"", GET_VEH_NAME(veh), capitalize(buf2));
      send_to_char(buf, ch);
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
      for (vict = veh->people; vict; vict = vict->next_in_veh) {
        if (success > 0) {
          suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
          if (suc > 0)
            snprintf(buf, sizeof(buf), "From outside, $z^n says into the vehicle in %s, \"%s^n\"\r\n", skills[GET_LANGUAGE(ch)].name, capitalize(buf2));
          else
            snprintf(buf, sizeof(buf), "From outside, $z^n speaks into the vehicle in a language you don't understand.\r\n");
        } else
          snprintf(buf, sizeof(buf), "From outside, $z mumbles incoherently into the vehicle.\r\n");
        store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_VICT));
      }
      return;
    }
    if (success > 0) {
      suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
      if (suc > 1)
        snprintf(buf, sizeof(buf), "$z %s you in %s, \"%s^n\"\r\n", action_plur, skills[GET_LANGUAGE(ch)].name, capitalize(buf2));
      else if (suc == 1)
        snprintf(buf, sizeof(buf), "$z %s in %s, but you don't understand.\r\n", action_plur, skills[GET_LANGUAGE(ch)].name);
      else
        snprintf(buf, sizeof(buf), "$z %s you in a language you don't understand.\r\n", action_plur);
    } else
      snprintf(buf, sizeof(buf), "$z %s to you incoherently.\r\n", action_plur);
    store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, 0, vict, TO_VICT));
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      snprintf(buf, sizeof(buf), "You %s $N, \"%s%s^n\"", action_sing, capitalize(buf2), (subcmd == SCMD_WHISPER) ? "" : "?");
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, 0, vict, TO_CHAR));
    }
    // TODO: Should this be stored to message history? It's super nondescript.
    act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
  }
}

ACMD(do_page)
{
  struct char_data *vict;

  half_chop(argument, arg, buf2);

  if (IS_NPC(ch))
    send_to_char("Monsters can't page.. go away.\r\n", ch);
  else if (!*arg)
    send_to_char("Whom do you wish to page?\r\n", ch);
  else {
    snprintf(buf, sizeof(buf), "\007\007*%s* %s", GET_CHAR_NAME(ch), buf2);
    if ((vict = get_char_vis(ch, arg)) != NULL) {
      if (vict == ch) {
        send_to_char("What's the point of that?\r\n", ch);
        return;
      }
      store_message_to_history(vict->desc, COMM_CHANNEL_PAGES, act(buf, FALSE, ch, 0, vict, TO_VICT));
      if (PRF_FLAGGED(ch, PRF_NOREPEAT))
        send_to_char(OK, ch);
      else
        store_message_to_history(ch->desc, COMM_CHANNEL_PAGES, act(buf, FALSE, ch, 0, vict, TO_CHAR));
      return;
    } else
      send_to_char("There is no such person in the game!\r\n", ch);
  }
}

ACMD(do_radio)
{
  struct obj_data *obj, *radio = NULL;
  char one[MAX_INPUT_LENGTH], two[MAX_INPUT_LENGTH];
  int i;
  bool cyberware = FALSE, vehicle = FALSE;

  if (ch->in_veh && (radio = GET_MOD(ch->in_veh, MOD_RADIO)))
    vehicle = 1;

  for (obj = ch->carrying; !radio && obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
      radio = obj;

  for (i = 0; !radio && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_RADIO)
        radio = GET_EQ(ch, i);
      else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
        for (struct obj_data *obj2 = GET_EQ(ch, i)->contains; obj2; obj2 = obj2->next_content)
          if (GET_OBJ_TYPE(obj2) == ITEM_RADIO)
            radio = obj2;
    }
  for (obj = ch->cyberware; !radio && obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_RADIO) {
      radio = obj;
      cyberware = 1;
    }

  if (!radio) {
    send_to_char("You don't have a radio.\r\n", ch);
    return;
  }
  any_one_arg(any_one_arg(argument, one), two);

  if (!*one) {
    act("$p:", FALSE, ch, radio, 0, TO_CHAR);
    if (GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) == -1)
      send_to_char("  Mode: scan\r\n", ch);
    else if (!GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))))
      send_to_char("  Mode: off\r\n", ch);
    else
      send_to_char(ch, "  Mode: center @ %d MHz\r\n",
                   GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))));
    if (GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
      send_to_char(ch, "  Crypt: on (level %d)\r\n",
                   GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))));
    else
      send_to_char("  Crypt: off\r\n", ch);
    return;
  } else if (!str_cmp(one, "off")) {
    act("You turn $p off.", FALSE, ch, radio, 0, TO_CHAR);
    GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = 0;
  } else if (!str_cmp(one, "scan")) {
    act("You set $p to scanning mode.", FALSE, ch, radio, 0, TO_CHAR);
    GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = -1;
    WAIT_STATE(ch, 16); /* Takes time to switch it */
  } else if (!str_cmp(one, "center")) {
    i = atoi(two);
    if (i > MAX_RADIO_FREQUENCY) {
      snprintf(buf, sizeof(buf), "$p cannot center a frequency higher than %d MHz.", MAX_RADIO_FREQUENCY);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
    }
    else if (i < MIN_RADIO_FREQUENCY) {
      snprintf(buf, sizeof(buf), "$p cannot center a frequency lower than %d MHz.", MIN_RADIO_FREQUENCY);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
    }
    else {
      snprintf(buf, sizeof(buf), "$p is now centered at %d MHz.", i);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
      GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = i;
      WAIT_STATE(ch, 16); /* Takes time to adjust */
    }
  } else if (!str_cmp(one, "crypt")) {
    if ((i = atoi(two))) {
      int max_crypt = GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 3 : 2)));
      if (i > max_crypt) {
        snprintf(buf, sizeof(buf), "$p's max crypt rating is %d.", max_crypt);
        act(buf, FALSE, ch, radio, 0, TO_CHAR);
      }
      else {
        send_to_char(ch, "Crypt mode enabled at rating %d..\r\n", max_crypt);
        GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))) = i;
      }
    }
    else {
      if (!GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
        act("$p's crypt mode is already disabled.", FALSE, ch, radio, 0, TO_CHAR);
      else {
        send_to_char("Crypt mode disabled.\r\n", ch);
        GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))) = 0;
      }
    }
  } else if (!str_cmp(one, "mode")) {
    if (GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) == -1)
      send_to_char(ch, "Your radio is currently scanning all frequencies. You can change the mode with ^WRADIO CENTER <frequency>, or turn it off with ^WRADIO OFF^n^n.\r\n");
    else if (!GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))))
      send_to_char(ch, "Your radio is currently off. You can turn it on with ^WRADIO CENTER <frequency>^n or ^WRADIO SCAN^n.\r\n");
    else
      send_to_char(ch, "Your radio is currently centered at %d MHz. You can change the mode with ^WRADIO SCAN^n, or turn it off with ^WRADIO OFF^n.\r\n",
                   GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))));
  } else
    send_to_char("That's not a valid option.\r\n", ch);
}

ACMD(do_broadcast)
{
  struct obj_data *obj, *radio = NULL;
  struct descriptor_data *d;
  int i, j, frequency, to_room, crypt, decrypt, success, suc;
  char buf3[MAX_STRING_LENGTH], buf4[MAX_STRING_LENGTH], voice[16] = "$v"; 
  bool cyberware = FALSE, vehicle = FALSE;
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char("You must be Authorized to do that. Until then, you can use the ^WNEWBIE^n channel if you need help.\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You can't manipulate electronics from the astral plane.\r\n", ch);
    return;
  }

  if (ch->in_veh && (radio = GET_MOD(ch->in_veh, MOD_RADIO)))
    vehicle = TRUE;

  for (obj = ch->carrying; !radio && obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
      radio = obj;

  for (i = 0; !radio && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_RADIO)
        radio = GET_EQ(ch, i);
      else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
        for (struct obj_data *obj2 = GET_EQ(ch, i)->contains; obj2; obj2 = obj2->next_content)
          if (GET_OBJ_TYPE(obj2) == ITEM_RADIO)
            radio = obj2;
    }
  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_RADIO && !radio) {
      radio = obj;
      cyberware = 1;
    } else if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD && GET_OBJ_VAL(obj, 3))
      snprintf(voice, sizeof(voice), "A masked voice");

  if (IS_NPC(ch) || IS_SENATOR(ch)) {
    argument = any_one_arg(argument, arg);
    if (!str_cmp(arg,"all") && !IS_NPC(ch))
      frequency = -1;
    else {
      frequency = atoi(arg);
      if (frequency < MIN_RADIO_FREQUENCY || frequency > MAX_RADIO_FREQUENCY) {
        send_to_char(ch, "Frequency must range between %d and %d.\r\n", MIN_RADIO_FREQUENCY, MAX_RADIO_FREQUENCY);
        return;
      }
    }
  } else if (!radio) {
    send_to_char("You have to have a radio to do that!\r\n", ch);
    return;
  } else if (!GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)))) {
    act("$p must be on in order to broadcast.", FALSE, ch, radio, 0, TO_CHAR);
    return;
  } else if (GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) == -1) {
    act("$p can't broadcast while scanning.", FALSE, ch, radio, 0, TO_CHAR);
    return;
  } else
    frequency = GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)));

  if (PLR_FLAGGED(ch, PLR_NOSHOUT)) {
    send_to_char("You aren't allowed to broadcast!\r\n", ch);
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("What do you want to broadcast?\r\n", ch);
    return;
  }

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  if (radio && GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
    crypt = GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3)));
  else
    crypt = 0;

  success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);

  STRCPY(buf4, argument);
  // Starting at end of buf, work backwards and fuzz out the message.
  for (int len = strlen(buf4) - 1; len >= 0; len--)
    switch (number(0, 2)) {
    case 1:
      buf4[len] = '.';
      break;
    case 2:
      buf4[len] = ' ';
      break;
    }
  snprintf(buf3, MAX_STRING_LENGTH, "*static* %s", buf4);
  if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_NO_RADIO))
    STRCPY(argument, buf3);

  
  if ( frequency > 0 ) {
    if(crypt) {
      snprintf(buf, sizeof(buf), "^y\\%s^y/[%d MHz, %s](CRYPTO-%d): %s^N", voice, frequency, skills[GET_LANGUAGE(ch)].name, crypt, capitalize(argument));
      snprintf(buf2, MAX_STRING_LENGTH, "^y\\Garbled Static^y/[%d MHz, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***^N", frequency, crypt);
      snprintf(buf4, MAX_STRING_LENGTH, "^y\\Unintelligible Voice^y/[%d MHz, Unknown](CRYPTO-%d): %s", frequency, crypt, capitalize(buf3));
      snprintf(buf3, MAX_STRING_LENGTH, "^y\\%s^y/[%d MHz, Unknown](CRYPTO-%d): (something incoherent...)^N", voice, frequency, crypt);
    } else {
      snprintf(buf, sizeof(buf), "^y\\%s^y/[%d MHz, %s]: %s^N", voice, frequency, skills[GET_LANGUAGE(ch)].name, capitalize(argument));
      snprintf(buf2, MAX_STRING_LENGTH, "^y\\%s^y/[%d MHz, Unknown]: %s^N", voice, frequency, capitalize(argument));
      snprintf(buf4, MAX_STRING_LENGTH, "^y\\Unintelligible Voice^y/[%d MHz, Unknown]: %s", frequency, capitalize(buf3));
      snprintf(buf3, MAX_STRING_LENGTH, "^y\\%s^y/[%d MHz, Unknown]: (something incoherent...)^N", voice, frequency);
    }

  } else {
    if(crypt) {
      snprintf(buf, sizeof(buf), "^y\\%s^y/[All Frequencies, %s](CRYPTO-%d): %s^N", voice, skills[GET_LANGUAGE(ch)].name, crypt, capitalize(argument));
      snprintf(buf2, MAX_STRING_LENGTH, "^y\\Garbled Static^y/[All Frequencies, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***^N", crypt);
      snprintf(buf4, MAX_STRING_LENGTH, "^y\\Unintelligible Voice^y/[All Frequencies, Unknown](CRYPTO-%d): %s", crypt, capitalize(buf3));
      snprintf(buf3, MAX_STRING_LENGTH, "^y\\%s^y/[All Frequencies, Unknown](CRYPTO-%d): (something incoherent...)^N", voice, crypt);
    } else {
      snprintf(buf, sizeof(buf), "^y\\%s^y/[All Frequencies, %s]: %s^N", voice, skills[GET_LANGUAGE(ch)].name, capitalize(argument));
      snprintf(buf2, MAX_STRING_LENGTH, "^y\\%s^y/[All Frequencies, Unknown]: %s^N", voice, capitalize(argument));
      snprintf(buf4, MAX_STRING_LENGTH, "^y\\Unintelligible Voice^y/[All Frequencies, Unknown]: %s", capitalize(buf3));
      snprintf(buf3, MAX_STRING_LENGTH, "^y\\%s^y/[All Frequencies, Unknown]: (something incoherent...)^N", voice);
    }
  }


  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
    store_message_to_history(ch->desc, COMM_CHANNEL_RADIO, act(buf, FALSE, ch, 0, 0, TO_CHAR));
  if (!ROOM_FLAGGED(get_ch_in_room(ch), ROOM_SOUNDPROOF))
    for (d = descriptor_list; d; d = d->next) {
      if (!d->connected && d != ch->desc && d->character &&
          !PLR_FLAGS(d->character).AreAnySet(PLR_WRITING,
                                             PLR_MAILING,
                                             PLR_EDITING, PLR_MATRIX, ENDBIT)
          && !IS_PROJECT(d->character) &&
          !ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_SOUNDPROOF) &&
          !ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_SENT)) {
        if (!IS_NPC(d->character) && !IS_SENATOR(d->character)) {
          radio = NULL;
          cyberware = FALSE;
          vehicle = FALSE;

          if (d->character->in_veh && (radio = GET_MOD(d->character->in_veh, MOD_RADIO)))
            vehicle = TRUE;

          for (obj = d->character->cyberware; obj && !radio; obj = obj->next_content)
            if (GET_OBJ_VAL(obj, 0) == CYB_RADIO) {
              radio = obj;
              cyberware = 1;
            }

          for (obj = d->character->carrying; obj && !radio; obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
              radio = obj;

          for (i = 0; !radio && i < NUM_WEARS; i++)
            if (GET_EQ(d->character, i)) {
              if (GET_OBJ_TYPE(GET_EQ(d->character, i)) == ITEM_RADIO)
                radio = GET_EQ(d->character, i);
              else if (GET_OBJ_TYPE(GET_EQ(d->character, i)) == ITEM_WORN
                       && GET_EQ(d->character, i)->contains)
                for (struct obj_data *obj2 = GET_EQ(d->character, i)->contains;
                     obj2; obj2 = obj2->next_content)
                  if (GET_OBJ_TYPE(obj2) == ITEM_RADIO)
                    radio = obj2;
            }
          FOR_ITEMS_AROUND_CH(ch, obj) {
            if (GET_OBJ_TYPE(obj) == ITEM_RADIO && !CAN_WEAR(obj, ITEM_WEAR_TAKE))
              radio = obj;
          }

          if (radio) {
            suc = success_test(GET_SKILL(d->character, GET_LANGUAGE(ch)), 4);
            if (CAN_WEAR(radio, ITEM_WEAR_EAR) || cyberware || vehicle)
              to_room = 0;
            else
              to_room = 1;

            i = GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)));
            j = GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 2 : 1)));
            decrypt = GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 3 : 2)));
            if (i == 0 || ((i != -1 && frequency != -1) && !(frequency >= (i - j) && frequency <= (i + j))))
              continue;
            if (decrypt >= crypt) {
              if (to_room) {
                if (success > 0 || IS_NPC(ch))
                  if (suc > 0 || IS_NPC(ch))
                    if (ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_NO_RADIO))
                      store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf4, FALSE, ch, 0, d->character, TO_VICT));
                    else
                      store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf, FALSE, ch, 0, d->character, TO_VICT));
                  else
                    store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf3, FALSE, ch, 0, d->character, TO_VICT));
                else
                  store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf3, FALSE, ch, 0, d->character, TO_VICT));
              } else {
                if (success > 0 || IS_NPC(ch))
                  if (suc > 0 || IS_NPC(ch))
                    if (ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_NO_RADIO))
                      store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf4, FALSE, ch, 0, d->character, TO_VICT));
                    else 
                      store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf, FALSE, ch, 0, d->character, TO_VICT));
                  else
                    store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf3, FALSE, ch, 0, d->character, TO_VICT));
                else
                  store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf3, FALSE, ch, 0, d->character, TO_VICT));
              }
            } else
              store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf2, FALSE, ch, 0, d->character, TO_VICT));
          }
        } else if (IS_SENATOR(d->character) && !PRF_FLAGGED(d->character, PRF_NORADIO))
          store_message_to_history(d, COMM_CHANNEL_RADIO, act(buf, FALSE, ch, 0, d->character, TO_VICT));
      }
    }

  for (d = descriptor_list; d; d = d->next)
    if (!d->connected &&
        d->character &&
        ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_SENT))
      ROOM_FLAGS(get_ch_in_room(d->character)).RemoveBit(ROOM_SENT);
}

/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

extern int _NO_OOC_;

ACMD(do_gen_comm)
{
  struct veh_data *veh;
  struct descriptor_data *i;
  struct descriptor_data *d;
  int channel = 0;

  static int channels[] = {
                            PRF_DEAF,
                            PRF_NONEWBIE,
                            PRF_NOOOC,
                            PRF_NORPE,
                            PRF_NOHIRED
                          };

  /*
   * com_msgs: [0] Message if you can't perform the action because of noshout
   *           [1] name of the action
   *           [2] message if you're not on the channel
   *           [3] a color string.
   */
  static const char *com_msgs[][6] = {
                                       {"You cannot shout!!\r\n",
                                        "shout",
                                        "Turn off your noshout flag first!\r\n",
                                        "^Y",
                                        B_YELLOW
                                       },

                                       {"You can't use the newbie channel!\r\n",
                                        "newbie",
                                        "You've turned that channel off!\r\n",
                                        "^G",
                                        B_GREEN},

                                       {"You can't use the OOC channel!\r\n",
                                        "ooc",
                                        "You have the OOC channel turned off.\n\r",
                                        "^n",
                                        KNUL},

                                       {"You can't use the RPE channel!\r\n",
                                        "rpe",
                                        "You have the RPE channel turned off.\r\n",
                                        B_RED},

                                       {"You can't use the hired channel!\r\n",
                                        "hired",
                                        "You have the hired channel turned off.\r\n",
                                        B_YELLOW}
                                     };

  /* to keep pets, etc from being ordered to shout */
  if (!ch->desc && !MOB_FLAGGED(ch, MOB_SPEC))
    return;

  if(PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && subcmd != SCMD_NEWBIE) {
    send_to_char(ch, "You must be Authorized to use that command. Until then, you can use the ^WNEWBIE^n channel if you need help.\r\n");
    return;
  }

  if (subcmd == SCMD_SHOUT && !char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  if (IS_ASTRAL(ch)) {
    send_to_char(ch, "Astral beings cannot %s.\r\n", com_msgs[subcmd][1]);
    return;
  }

  if ( _NO_OOC_ && subcmd == SCMD_OOC ) {
    send_to_char("This command has been disabled.\r\n",ch);
    return;
  }

  if ((PLR_FLAGGED(ch, PLR_NOSHOUT) && subcmd == SCMD_SHOUT) ||
      (PLR_FLAGGED(ch, PLR_NOOOC) && subcmd == SCMD_OOC) ||
      (!(PLR_FLAGGED(ch, PLR_RPE) || IS_SENATOR(ch))  && subcmd == SCMD_RPETALK) ||
      (!(PRF_FLAGGED(ch, PRF_QUEST) || IS_SENATOR(ch))  && subcmd == SCMD_HIREDTALK)) {
    send_to_char(com_msgs[subcmd][0], ch);
    return;
  }

  if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_SOUNDPROOF) && subcmd == SCMD_SHOUT) {
    send_to_char("The walls seem to absorb your words.\r\n", ch);
    return;
  }

  /* make sure the char is on the channel */
  if (PRF_FLAGGED(ch, channels[subcmd])) {
    send_to_char(com_msgs[subcmd][2], ch);
    return;
  }

  if (subcmd == SCMD_NEWBIE) {
    if (IS_NPC(ch)) {
      send_to_char("No.\r\n", ch);
      return;
    } else if (!IS_SENATOR(ch) && !PLR_FLAGGED(ch, PLR_NEWBIE) && !PRF_FLAGGED(ch, PRF_NEWBIEHELPER)) {
      send_to_char("You are too experienced to use the newbie channel.\r\n", ch);
      return;
    }
  }

  skip_spaces(&argument);

  /* make sure that there is something there to say! */
  if (!*argument) {
    send_to_char(ch, "Yes, %s, fine, %s we must, but WHAT???\r\n", com_msgs[subcmd][1], com_msgs[subcmd][1]);
    return;
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else if (subcmd == SCMD_SHOUT) {
    struct room_data *was_in = NULL;
    struct char_data *tmp;
    int success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
    for (tmp = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room))
      if (tmp != ch) {
        if (success > 0) {
          int suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
          if (suc > 0 || IS_NPC(tmp))
            snprintf(buf, sizeof(buf), "%s$z shouts in %s, \"%s%s\"^n", com_msgs[subcmd][3], skills[GET_LANGUAGE(ch)].name, capitalize(argument), com_msgs[subcmd][3]);
          else
            snprintf(buf, sizeof(buf), "%s$z shouts in a language you don't understand.", com_msgs[subcmd][3]);
        } else
          STRCPY(buf, "$z shouts incoherently.");
        if (IS_NPC(ch))
          snprintf(buf, sizeof(buf), "%s$z shouts, \"%s%s\"^n", com_msgs[subcmd][3], capitalize(argument), com_msgs[subcmd][3]);
        
        // Note that this line invokes act().
        store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
      }

    snprintf(buf1, MAX_STRING_LENGTH,  "%sYou shout, \"%s%s\"^n", com_msgs[subcmd][3], capitalize(argument), com_msgs[subcmd][3]);
    // Note that this line invokes act().
    store_message_to_history(ch->desc, COMM_CHANNEL_SHOUTS, act(buf1, FALSE, ch, 0, 0, TO_CHAR));

    was_in = ch->in_room;
    if (ch->in_veh) {
      ch->in_room = get_ch_in_room(ch);
      snprintf(buf1, MAX_STRING_LENGTH,  "%sFrom inside %s, $z %sshouts, \"%s%s\"^n", com_msgs[subcmd][3], GET_VEH_NAME(ch->in_veh),
              com_msgs[subcmd][3], capitalize(argument), com_msgs[subcmd][3]);
      for (tmp = ch->in_room->people; tmp; tmp = tmp->next_in_room) {
        // Replicate act() in a way that lets us capture the message.
        if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
          // They're a valid target, so send the message with a raw perform_act() call.
          store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, perform_act(buf1, ch, NULL, NULL, tmp));
        }
      }
    } else {
      for (veh = ch->in_room->vehicles; veh; veh = veh->next_veh) {
        ch->in_veh = veh;
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, perform_act(buf, ch, NULL, NULL, tmp));
          }
        }
        ch->in_veh = NULL;
      }
    }

    for (int door = 0; door < NUM_OF_DIRS; door++)
      if (CAN_GO(ch, door)) {
        ch->in_room = ch->in_room->dir_option[door]->to_room;
        for (tmp = get_ch_in_room(ch)->people; tmp; tmp = tmp->next_in_room)
          if (tmp != ch) {
            if (success > 0) {
              int suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                snprintf(buf, sizeof(buf), "%s$z shouts in %s, \"%s%s\"^n", com_msgs[subcmd][3], skills[GET_LANGUAGE(ch)].name, capitalize(argument), com_msgs[subcmd][3]);
              else
                snprintf(buf, sizeof(buf), "%s$z shouts in a language you don't understand.", com_msgs[subcmd][3]);
            } else
              STRCPY(buf, "$z shouts incoherently.");
            if (IS_NPC(ch))
              snprintf(buf, sizeof(buf), "%s$z shouts, \"%s%s\"^n", com_msgs[subcmd][3], capitalize(argument), com_msgs[subcmd][3]);
            
            // If it sent successfully, store to their history.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
          }
        ch->in_room = was_in;
      }
    if (ch->in_veh)
      ch->in_room = NULL;

    return;
  } else if(subcmd == SCMD_OOC) {
    /* Check for non-switched mobs */
    if ( IS_NPC(ch) && ch->desc == NULL )
      return;
    delete_doubledollar(argument);
    for ( d = descriptor_list; d != NULL; d = d->next ) {
      if (!d->character || found_mem(GET_IGNORE(d->character), ch))
        continue;
      if (!access_level(d->character, LVL_BUILDER) 
          && ((d->connected != CON_PLAYING && !PRF_FLAGGED(d->character, PRF_MENUGAG))
              || PLR_FLAGGED( d->character, PLR_WRITING) 
              || PRF_FLAGGED( d->character, PRF_NOOOC) 
              || PLR_FLAGGED(d->character, PLR_NOT_YET_AUTHED)))
        continue;

      if (!access_level(d->character, GET_INCOG_LEV(ch)))
        snprintf(buf, sizeof(buf), "^m[^nSomeone^m]^n ^R(^nOOC^R)^n, \"%s^n\"\r\n", capitalize(argument));
      else
        snprintf(buf, sizeof(buf), "^m[^n%s^m]^n ^R(^nOOC^R)^n, \"%s^n\"\r\n", IS_NPC(ch) ? GET_NAME(ch) : GET_CHAR_NAME(ch), capitalize(argument));
      
      store_message_to_history(d, COMM_CHANNEL_OOC, buf);
      send_to_char(buf, d->character);
    }

    return;
  } else if (subcmd == SCMD_RPETALK) {
    snprintf(buf, sizeof(buf), "%s%s ^W[^rRPE^W]^r %s^n\r\n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), capitalize(argument));
    
    channel = COMM_CHANNEL_RPE;
    store_message_to_history(ch->desc, channel, buf);
  } else if (subcmd == SCMD_HIREDTALK) {
    snprintf(buf, sizeof(buf), "%s%s ^y[^YHIRED^y]^Y %s^n\r\n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), capitalize(argument));
    
    channel = COMM_CHANNEL_HIRED;
    store_message_to_history(ch->desc, channel, buf);
  } else {
    snprintf(buf, sizeof(buf), "%s%s |]newbie[| %s^n\r\n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), capitalize(argument));
    send_to_char(buf, ch);
    
    channel = COMM_CHANNEL_NEWBIE;
    store_message_to_history(ch->desc, channel, buf);
  }
  
  for (i = descriptor_list; i; i = i->next) {
    if (i == ch->desc || !i->character || IS_PROJECT(i->character))
      continue;
      
    if (PRF_FLAGGED(i->character, channels[subcmd]))
      continue;
      
    if (!IS_SENATOR(i->character)) {
      if (i->connected || PLR_FLAGS(i->character).AreAnySet(PLR_WRITING, PLR_MAILING, PLR_EDITING, ENDBIT))
        continue;
    }
      
    switch (subcmd) {
      case SCMD_SHOUT:
        if (ROOM_FLAGGED(get_ch_in_room(i->character), ROOM_SOUNDPROOF))
          continue;
        break;
      case SCMD_NEWBIE:
        if (!(PLR_FLAGGED(i->character, PLR_NEWBIE) || IS_SENATOR(i->character) || PRF_FLAGGED(i->character, PRF_NEWBIEHELPER)))
          continue;
        break;
      case SCMD_RPETALK:
        if (!(PLR_FLAGGED(i->character, PLR_RPE) || IS_SENATOR(i->character)))
          continue;
        break;
      case SCMD_HIREDTALK:
        if (!(PRF_FLAGGED(i->character, PRF_QUEST) || IS_SENATOR(i->character)))
          continue;
        break;
    }
    send_to_char(buf, i->character);
    store_message_to_history(i, channel, buf);
  }

  /* now send all the strings out 
  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i != ch->desc && i->character &&
        !PRF_FLAGGED(i->character, channels[subcmd]) &&
        !PLR_FLAGS(i->character).AreAnySet(PLR_WRITING,
                                           PLR_MAILING,
                                           PLR_EDITING, ENDBIT) &&
        !IS_PROJECT(i->character) &&
        !(ROOM_FLAGGED(get_ch_in_room(i->character), ROOM_SOUNDPROOF) && subcmd == SCMD_SHOUT)) {
      if (subcmd == SCMD_NEWBIE && !(PLR_FLAGGED(i->character, PLR_NEWBIE) ||
                                     IS_SENATOR(i->character) || PRF_FLAGGED(i->character, PRF_NEWBIEHELPER)))
        continue;
      else if (subcmd == SCMD_RPETALK && !(PLR_FLAGGED(i->character, PLR_RPE) ||
                                           IS_SENATOR(i->character)))
        continue;
      else if (subcmd == SCMD_HIREDTALK && !(PRF_FLAGGED(i->character, PRF_QUEST) ||
                                             IS_SENATOR(i->character)))
        continue;
      else {
        // Note that this invokes act().
        store_message_to_history(i, channel, act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP));
      }
    } */
}

ACMD(do_language)
{
  int i, lannum;
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("You know the following languages:\r\n", ch);
    for (i = SKILL_ENGLISH; i <= SKILL_FRENCH; i++)
      if ((GET_SKILL(ch, i)) > 0) {
        snprintf(buf, sizeof(buf), "%-20s %-17s", skills[i].name, how_good(i, GET_SKILL(ch, i)));
        if (GET_LANGUAGE(ch) == i)
          STRCAT(buf, " ^Y(Speaking)^n");
        STRCAT(buf, "\r\n");
        send_to_char(buf, ch);
      }
    return;
  }

  if ((lannum = find_skill_num(arg)) && (lannum >= SKILL_ENGLISH && lannum <= SKILL_FRENCH))
    if (GET_SKILL(ch, lannum) > 0) {
      GET_LANGUAGE(ch) = lannum;
      send_to_char(ch, "You will now speak %s.\r\n", skills[lannum].name);
    } else
      send_to_char("You don't know how to speak that language.\r\n", ch);
  else
    send_to_char("Invalid Language.\r\n", ch);

}

void add_phone_to_list(struct obj_data *obj)
{
  struct phone_data *k;
  bool found = FALSE, cyber = FALSE;
  if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE)
    cyber = TRUE;
  snprintf(buf, sizeof(buf), "%04d%04d", GET_OBJ_VAL(obj, (cyber ? 3 : 0)), GET_OBJ_VAL(obj, (cyber ? 6 : 1)));
  for (struct phone_data *j = phone_list; j; j = j->next)
    if (j->number == atoi(buf)) {
      found = TRUE;
      break;
    }
  if (!found) {
    k = new phone_data;
    k->number = atoi(buf);
    k->phone = obj;
    if (phone_list)
      k->next = phone_list;
    phone_list = k;
  }  
}

ACMD(do_phone)
{
  struct obj_data *obj;
  struct char_data *tch = FALSE;
  struct phone_data *k = NULL, *phone, *temp;
  bool cyber = FALSE;
  sh_int active = 0;
  int ring = 0;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_PHONE)
      break;
  for (int x = 0; !obj && x < NUM_WEARS; x++)
    if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_PHONE)
      obj = GET_EQ(ch, x);
  if (!obj)
    for (obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_PHONE) {
        cyber = TRUE;
        break;
      }
  if (!obj) {
    send_to_char("But you don't have a phone.\r\n", ch);
    return;
  }
  for (phone = phone_list; phone; phone = phone->next)
    if (phone->phone == obj)
      break;

  if (subcmd == SCMD_ANSWER) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (phone->connected) {
      send_to_char("But you already have a call connected!\r\n", ch);
      return;
    }
    if (!phone->dest) {
      send_to_char("No one is calling you, what a shame.\r\n", ch);
      return;
    }
    if (phone->dest->persona)
      send_to_icon(phone->dest->persona, "The call is answered.\r\n");
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
      if (tch)
        send_to_char("The phone is answered.\r\n", tch);
    }
    send_to_char("You answer the phone.\r\n", ch);
    phone->connected = TRUE;
    phone->dest->connected = TRUE;
  } else if (subcmd == SCMD_RING) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (phone->dest) {
      send_to_char("But you already have a call connected!\r\n", ch);
      return;
    }
    any_one_arg(argument, arg);
    if (!*arg) {
      send_to_char("Ring what number?\r\n", ch);
      return;
    }
    if (!(ring = atoi(arg))) {
      send_to_char("That is not a valid number.\r\n", ch);
      return;
    }

    for (k = phone_list; k; k = k->next)
      if (k->number == ring)
        break;

    if (!k) {
      send_to_char("The phone is picked up and a polite female voice says, \"This number is not in"
                   " service, please check your number and try again.\"\r\n", ch);
      return;
    }
    if (k == phone) {
      send_to_char("Why would you want to call yourself?\r\n", ch);
      return;
    }
    if (k->dest) {
      send_to_char("You hear the busy signal.\r\n", ch);
      return;
    }
    phone->dest = k;
    phone->connected = TRUE;
    k->dest = phone;

    if (k->persona) {
      send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
    } else {
      tch = k->phone->carried_by ? k->phone->carried_by : k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by ? k->phone->in_obj->carried_by : k->phone->in_obj->worn_by;
      if (tch) {
        if (GET_POS(tch) == POS_SLEEPING) {
          if (success_test(GET_WIL(tch), 4)) { // Why does it take a successful Willpower test to hear your phone?
            GET_POS(tch) = POS_RESTING;
            send_to_char("You are woken by your phone ringing.\r\n", tch);
            if (!GET_OBJ_VAL(k->phone, 3))
              act("$n is startled awake by the ringing of $s phone.", FALSE, tch, 0, 0, TO_ROOM);
          } else if (!GET_OBJ_VAL(k->phone, 3))
            act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
        } else if (!GET_OBJ_VAL(k->phone, 3)) {
          act("Your phone rings.", FALSE, tch, 0, 0, TO_CHAR);
          act("$n's phone rings.", FALSE, tch, NULL, NULL, TO_ROOM);
        } else {
          if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
            act("You feel your phone ring.", FALSE, tch, 0, 0, TO_CHAR);
        }
      } else {
        snprintf(buf, sizeof(buf), "%s rings.", GET_OBJ_NAME(k->phone));
        if (k->phone->in_room || k->phone->in_veh)
          act(buf, FALSE, NULL, k->phone, 0, TO_ROOM);
        // Edge case: A phone inside a container inside a container won't ring. But do we even want it to?
      }
    }
    send_to_char("It begins to ring.\r\n", ch);
  } else if (subcmd == SCMD_HANGUP) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (!phone->dest) {
      send_to_char("But you're not talking to anyone.\r\n", ch);
      return;
    }
    if (phone->dest->persona)
      send_to_icon(phone->dest->persona, "The flashing phone icon fades from view.\r\n");
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
      if (tch) {
        if (phone->dest->connected)
          send_to_char("The phone is hung up from the other side.\r\n", tch);
        else
          act("Your phone stops ringing.\r\n", FALSE, tch, 0, 0, TO_CHAR);
      }
    }
    phone->connected = FALSE;
    phone->dest->dest = NULL;
    phone->dest->connected = FALSE;
    phone->dest = NULL;
    send_to_char("You end the call.\r\n", ch);
  } else if (subcmd == SCMD_TALK) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (!phone->connected) {
      send_to_char("But you don't currently have a call.\r\n", ch);
      return;
    }
    if (!phone->dest->connected) {
      send_to_char(ch, "No one has answered it yet.\r\n");
      return;
    }
    if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
      return;
    
    skip_spaces(&argument);
    #define VOICE_BUF_SIZE 20
    char voice[VOICE_BUF_SIZE] = "$v";
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD && GET_OBJ_VAL(obj, 3))
        snprintf(voice, VOICE_BUF_SIZE, "A masked voice");
    
    if (success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4) > 0) {
      snprintf(buf, sizeof(buf), "^Y%s on the other end of the line says in %s, \"%s\"", voice, skills[GET_LANGUAGE(ch)].name, capitalize(argument));
      snprintf(buf2, MAX_STRING_LENGTH, "$z says into $s phone in %s, \"%s\"", skills[GET_LANGUAGE(ch)].name, capitalize(argument));
    } else {
      snprintf(buf, sizeof(buf), "^Y$v on the other end of the line mumbles incoherently.");
      snprintf(buf2, MAX_STRING_LENGTH, "$z mumbles incoherently into $s phone.\r\n");
    }
    snprintf(buf3, MAX_STRING_LENGTH, "^YYou say, \"%s\"\r\n", capitalize(argument));
    send_to_char(buf3, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, buf3);
    if (phone->dest->persona && phone->dest->persona->decker && phone->dest->persona->decker->ch)
      store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, act(buf, FALSE, ch, 0, phone->dest->persona->decker->ch, TO_DECK));
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
    }
    if (tch) {
      if (success_test(GET_SKILL(tch, GET_LANGUAGE(ch)), 4) > 0)
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, act(buf, FALSE, ch, 0, tch, TO_VICT));
      else
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, act("^Y$v speaks in a language you don't understand.", FALSE, ch, 0, tch, TO_VICT));
    }
    if (!cyber) {
      for (tch = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tch; tch = ch->in_veh ? tch->next_in_veh : tch->next_in_room)
        if (tch != ch) {
          if (success_test(GET_SKILL(tch, GET_LANGUAGE(ch)), 4) > 0)
            store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, act(buf2, FALSE, ch, 0, tch, TO_VICT));
          else
            store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, act("$z speaks into $s phone in a language you don't understand.", FALSE, ch, 0, tch, TO_VICT));
        }
    }
  } else {
    any_one_arg(argument, arg);
    if (!str_cmp(arg, "off"))
      active = 1;
    else if (!str_cmp(arg, "on"))
      active = 2;
    else if (!str_cmp(arg, "ring"))
      ring = 1;
    else if (!str_cmp(arg, "silent"))
      ring = 2;
    if (ring) {
      ring--;
      if (GET_OBJ_VAL(obj, (cyber ? 8 : 3)) == ring) {
        send_to_char(ch, "It's already set to %s.\r\n", ring ? "silent" : "ring");
        return;
      }
      send_to_char(ch, "You set %s to %s.\r\n", GET_OBJ_NAME(obj), ring ? "silent" : "ring");
      GET_OBJ_VAL(obj, (cyber ? 8 : 3)) = ring;
      return;
    }
    if (active) {
      if (phone && phone->dest) {
        send_to_char("You might want to hang up first.\r\n", ch);
        return;
      }
      active--;
      if (GET_OBJ_VAL(obj, (cyber ? 7 : 2)) == active) {
        send_to_char(ch, "It's already switched %s.\r\n", active ? "on" : "off");
        return;
      }
      send_to_char(ch, "You switch %s %s.\r\n", GET_OBJ_NAME(obj), active ? "on" : "off");
      GET_OBJ_VAL(obj, (cyber ? 7 : 2)) = active;
      if (active)
        add_phone_to_list(obj);
      else {
        if (!phone) {
          send_to_char("Uh WTF M8?\r\n", ch);
          snprintf(buf, sizeof(buf), "%s would have crashed the mud, whats up with their phone.", GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          return;
        }
        REMOVE_FROM_LIST(phone, phone_list, next);
        DELETE_AND_NULL(phone);
      }
      return;
    }
    snprintf(buf, MAX_STRING_LENGTH, 
            "%s:\r\n"
            "Phone Number: %04d-%04d\r\n"
            "Switched: %s\r\n"
            "Ringing: %s\r\n", 
            GET_OBJ_NAME(obj),
            GET_OBJ_VAL(obj, (cyber ? 3 : 0)),
            GET_OBJ_VAL(obj, (cyber ? 6 : 1)),
            GET_OBJ_VAL(obj, (cyber ? 7 : 2)) ? "On" : "Off",
            GET_OBJ_VAL(obj, (cyber ? 8 : 3)) ? "Off": "On"
    );
    
    if (phone && phone->dest) {
      if (phone->dest->connected && phone->connected)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Connected to: %d\r\n", phone->dest->number);
      else if (!phone->dest->connected)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Calling: %d\r\n", phone->dest->number);
      else snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Incoming call from: %08d\r\n", phone->dest->number);
    } 
    send_to_char(buf, ch);
  }
}


ACMD(do_phonelist)
{
  struct phone_data *k;
  struct char_data *tch = NULL;
  int i = 0;

  if (!phone_list)
    send_to_char(ch, "The phone list is empty.\r\n");

  for (k = phone_list; k; k = k->next) {
    if (k->persona && k->persona->decker) {
      tch = k->persona->decker->ch;
    } else if (k->phone) {
      tch = k->phone->carried_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->worn_by;
    }
    send_to_char(ch, "%2d) %d (%s) (%s)\r\n", i, k->number, (k->dest ? "Busy" : "Free"),
            (tch ? (IS_NPC(tch) ? GET_NAME(tch) : GET_CHAR_NAME(tch)) : "no one"));
    i++;
  }
}

void phone_check()
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *tch;
  struct phone_data *k;
  for (k = phone_list; k; k = k->next) {
    if (k->dest && !k->connected) {
      if (k->persona) {
        send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
        continue;
      }
      tch = k->phone->carried_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->worn_by;
      if (tch) {
        if (GET_POS(tch) == POS_SLEEPING) {
          if (GET_POS(tch) == POS_SLEEPING) {
            if (success_test(GET_WIL(tch), 4)) { // Why does it take a successful Willpower test to hear your phone?
              GET_POS(tch) = POS_RESTING;
              send_to_char("You are woken by your phone ringing.\r\n", tch);
              if (!GET_OBJ_VAL(k->phone, 3))
                act("$n is startled awake by the ringing of $s phone.", FALSE, tch, 0, 0, TO_ROOM);
            } else if (!GET_OBJ_VAL(k->phone, 3))
              act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
          } else if (!GET_OBJ_VAL(k->phone, 3)) {
            act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
            continue;
          } else
            continue;
        }
        if (!GET_OBJ_VAL(k->phone, 3)) {
          act("Your phone rings.", FALSE, tch, 0, 0, TO_CHAR);
          act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
        } else {
          if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
            act("You feel your phone ring.", FALSE, tch, 0, 0, TO_CHAR);
        }
      } else {
        snprintf(buf, sizeof(buf), "%s rings.", GET_OBJ_NAME(k->phone));
        if (k->phone->in_room || k->phone->in_veh)
          act(buf, FALSE, NULL, k->phone, 0, TO_ROOM);
      }
    }
  }
}

ACMD(do_ignore)
{
  if (!*argument) {
    send_to_char("You are currently ignoring: \r\n", ch);
    for (struct remem *a = GET_IGNORE(ch); a; a = a->next) {
      char *name = get_player_name(a->idnum);
      if (name) {
        send_to_char(ch, "%s\r\n", name);
        DELETE_AND_NULL_ARRAY(name);
      }
    }
  } else {
    struct remem *list;
    skip_spaces(&argument);
    if (struct char_data *tch = get_player_vis(ch, argument, FALSE)) {
      if (GET_LEVEL(tch) > LVL_MORTAL)
        send_to_char("You can't ignore immortals.\r\n", ch);
      else if ((list = found_mem(GET_IGNORE(ch), tch))) {
        struct remem *temp;
        REMOVE_FROM_LIST(list, GET_IGNORE(ch), next);
        DELETE_AND_NULL(list);
        send_to_char(ch, "You can now hear %s.\r\n", GET_CHAR_NAME(tch));
      } else {
        list = new remem;
        list->idnum = GET_IDNUM(tch);
        list->next = GET_IGNORE(ch);
        GET_IGNORE(ch) = list;
        send_to_char(ch, "You will no longer hear %s.\r\n", GET_CHAR_NAME(tch));
      }
    } else send_to_char("They are not logged on.\r\n", ch);
  }  
}

void send_message_history_to_descriptor(struct descriptor_data *d, int channel, int maximum, const char* channel_string) {
  // Precondition: No screwy pointers. Allow for NPCs to be passed (we ignore them).
  if (d == NULL) {
    //mudlog("SYSERR: Null descriptor passed to send_message_history_to_descriptor.", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Precondition: Channel must be a valid index (0 ≤ channel < number of channels defined in awake.h).
  if (channel < 0 || channel >= NUM_COMMUNICATION_CHANNELS) {
    snprintf(buf, sizeof(buf), "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (maximum > NUM_MESSAGES_TO_RETAIN) {
    snprintf(buf, sizeof(buf), "SYSERR: send_message_history_to_descriptor asked to send %d messages, but max message history is %d.",
            maximum, NUM_MESSAGES_TO_RETAIN);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (maximum <= 0) {
    snprintf(buf, sizeof(buf), "SYSERR: send_message_history_to_descriptor asked to send %d messages, but minimum is 1.", maximum);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (d->message_history[channel].NumItems() == 0) {
    snprintf(buf, sizeof(buf), "You haven't heard any messages %s yet.\r\n", channel_string);
    write_to_output(buf, d);
    return;
  }
  
  int skip = d->message_history[channel].NumItems() - maximum;
  
  snprintf(buf, sizeof(buf), "The last %d messages you've heard %s are:\r\n", maximum ? maximum : NUM_MESSAGES_TO_RETAIN, channel_string);
  write_to_output(buf, d);
  
  // For every message in their history, print the list from oldest to newest.
  for (nodeStruct<const char*> *currnode = d->message_history[channel].Tail(); currnode; currnode = currnode->prev) {
    // If they've requested a maximum of X, then skip 20-X of the oldest messages before we start sending them.
    if (skip-- > 0)
      continue;
    
    snprintf(buf, sizeof(buf), "  %s", currnode->data);
    int size = strlen(buf);
    write_to_output(ProtocolOutput(d, buf, &size), d);
  }
}

void display_message_history_help(struct char_data *ch) {
  send_to_char("You have past messages available for the following channels:\r\n", ch);
  
  // Send the names of any channels that have messages waiting.
  for (int channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
    if (ch->desc->message_history[channel].NumItems() > 0)
      send_to_char(ch, "  %s\r\n", message_history_channels[channel]);
  
  send_to_char("\r\nSyntax: HISTORY <channel name> [optional number of messages to retrieve]\r\n", ch);
}

void raw_message_history(struct char_data *ch, int channel, int quantity) {
  switch (channel) {
    case COMM_CHANNEL_HIRED:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the Hired Talk channel");
      break;
    case COMM_CHANNEL_NEWBIE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the Newbie channel");
      break;
    case COMM_CHANNEL_OOC:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the OOC channel");
      break;
    case COMM_CHANNEL_OSAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "said OOCly");
      break;
    case COMM_CHANNEL_PAGES:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over pages");
      break;
    case COMM_CHANNEL_PHONE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the phone");
      break;
    case COMM_CHANNEL_RPE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the RPE Talk channel");
      break;
    case COMM_CHANNEL_RADIO:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the radio");
      break;
    case COMM_CHANNEL_SAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "spoken aloud");
      break;
    case COMM_CHANNEL_SHOUTS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "from nearby shouts");
      break;
    case COMM_CHANNEL_TELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over tells");
      break;
    case COMM_CHANNEL_WTELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over wiztells");
      break;
    default:
      snprintf(buf, sizeof(buf), "SYSERR: Unrecognized channel/subcmd %d provided to raw_message_history's channel switch.", channel);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      send_to_char(ch, "Sorry, this command appears to have an error. Staff have been notified.\r\n");
      break;
  }
}

ACMD(do_switched_message_history) {
  int channel = subcmd, quantity = NUM_MESSAGES_TO_RETAIN;
  
  assert(channel >= 0 && channel < NUM_COMMUNICATION_CHANNELS);
  
  if (*argument) {
    if ((quantity = atoi(argument)) > NUM_MESSAGES_TO_RETAIN) {
      send_to_char(ch, "Sorry, the system only retains up to %d messages.\r\n", NUM_MESSAGES_TO_RETAIN);
      // This is not a fatal error.
    }
    
    if (quantity < 1) {
      send_to_char("You must specify a number of history messages greater than 0.\r\n", ch);
      return;
    }
  }
  
  raw_message_history(ch, channel, quantity);
}

ACMD(do_message_history) {
  int channel, quantity = NUM_MESSAGES_TO_RETAIN;
  
  if (!*argument) {
    display_message_history_help(ch);
    return;
  }
  
  half_chop(argument, buf, buf2);
  
  // Find the channel referenced in first parameter.
  for (channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
    if (is_abbrev(buf, message_history_channels[channel]))
      break;
  
  // No channel found? Fail.
  if (channel >= NUM_COMMUNICATION_CHANNELS) {
    send_to_char(ch, "Sorry, '%s' is not a recognized channel.\r\n", buf);
    display_message_history_help(ch);
    return;
  }
  
  if (*buf2) {
    if ((quantity = atoi(buf2)) > NUM_MESSAGES_TO_RETAIN) {
      send_to_char(ch, "Sorry, the system only retains up to %d messages.\r\n", NUM_MESSAGES_TO_RETAIN);
      // This is not a fatal error.
    }
    
    if (quantity < 1) {
      send_to_char("You must specify a number of history messages greater than 0.\r\n", ch);
      return;
    }
  }
  
  raw_message_history(ch, channel, quantity);
}
