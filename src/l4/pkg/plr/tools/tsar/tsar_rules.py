#!/usr/bin/python

"""TSAR Event rewrite rule support"""

import os
import tempfile
import re
import sys

#pylint: disable=C0103,R0903


class Condition:
    """Represents a condition to check"""
    def __init__(self, cond):
        """Condition has the format: <attribute>:<value>
           where value can be an integer or a string (if surrounded
           with quotation marks."""
        (self.attrib, v) = cond.split(":")
        if v[0] == '"':
            self.value = v[1:-1]
            self.isint = False
            self.isstring = True
        else:
            self.value = int(v, 0)
            self.isint = True
            self.isstring = False

        #print self.attrib, " -- ", self.value

    def apply(self, obj):
        """Apply the condition to a given object.

        Checks if the object matches the (attribute,value)
        pair specified by this condition.
        """
        return getattr(obj, self.attrib) == self.value

    def __repr__(self):
        if self.isstring:
            return "Cond(%s == %s)" % (self.attrib, self.value)
        else:
            return "Cond(%s == 0x%x)" % (self.attrib, self.value)


class Action:
    """Represents a modification action"""
    def __init__(self, act):
        (self.dest, val) = act.split(":")
        self.deststring = (val[0] == "\"")
        if self.deststring:
            self.destval = val[1:-1]
        else:
            self.destval = int(val, 0)

    def action(self, obj):
        """Perform the write action"""
        #print "Rewriting: %s -> %s" % (self.dest, self.destval)
        setattr(obj, self.dest, self.destval)


class ProcessorRule:
    """Representation of a single rewrite rule.

    Rules come from a file and consist of a list of (conjunctive)
    conditions and a rewrite target.
        * Conditions are a list of <attr> : <val> pairs, separated
          by commas.
        * The rewrite target is a <attr> : <val> pair
        * Values can be integer values (any base encoding is
          supported) or strings (using quotation marks).
        * Conditions and rewrite target a separated using an arrow: ->
    """
    def __init__(self, _input):
        self.conditions = []
        self.actions = []
        #print _input
        #print "Conditions: "
        for cond in _input[0].split(","):
            self.conditions += [Condition(cond)]

        for act in _input[1].split(","):
            self.actions += [Action(act)]

    def apply(self, event):
        """Apply the rewrite rule to a given event object.

        If the event matches all conditions of this rule,
        the event's target attribute is rewritten.
        """
        for c in self.conditions:
            #print event, c
            if not c.apply(event):
                #print "Cond did not hold"
                return

        #print "Rewriting: %s -> %s" % (self.dest, self.destval)
        for act in self.actions:
            act.action(event)


class EventPreprocessor:
    """Event Preprocessor

    Takes a list of rewrite rules from a configuration file.
    These rewrite rules can then be applied to an event using
    the process() function.

    The configuration file is searched either under the name
    "tsar.rules" in the local directory or by using the
    TSAR_RULEFILE environment variable if it is set.
    """

    def tryLocalRuleFile(self):
        """Try to use local tsar.rules rule file."""
        try:
            self.rulefile = file("tsar.rules", "r")
        except IOError:
            self.rulefile = None

    def tryEnvRuleFile(self):
        """Try to find rule file using TSAR_RULEFILE
           environment variable.
        """
        try:
            envrule = os.getenv("TSAR_RULEFILE")
            if envrule == "":
                self.rulefile = tempfile.NamedTemporaryFile(mode="r")
            else:
                self.rulefile = file(envrule, "r")
        except IOError:
            self.rulefile = None
        except TypeError:
            self.rulefile = None

    def parseRules(self):
        """Read rules from the rule file found."""
        rules = []
        for l in self.rulefile.readlines():

            # skip comments
            if l.strip().startswith("#"):
                continue

            l = l.replace(" ", "")
            m = re.match("(.*)->(.*)", l.strip())
            if m:
                rules += [ProcessorRule(m.groups())]
        return rules

    def __init__(self):
        self.rulefile = None
        self.tryLocalRuleFile()
        if self.rulefile is None:
            self.tryEnvRuleFile()
        if self.rulefile is None:
            sys.stderr.write("No rule file found. "
                             "Use TSAR_FULEFILE='' to use none.\n")
            sys.exit(1)

        self.rules = self.parseRules()

    def process(self, event):
        """Apply the rewrite rules to the given event."""
        for r in self.rules:
            r.apply(event)
        return event
