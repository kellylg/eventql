/**
 * Copyright (c) 2017 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
 *   - Laura Schlimmer <laura@eventql.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */

EventQL = this.EventQL || {};

EventQL.SVGHelper = function() {
'use strict';

  this.svg = "";

  this.drawLine = function(x0, x1, y0, y1, classes) {
    this.svg +=
        "<line" +
            xmlAttr("x1", x0) +
            xmlAttr("x2", x1) +
            xmlAttr("y1", y0) +
            xmlAttr("y2", y1) +
            xmlAttr("class", classes) +
        " />";
  };

  this.drawText = function(x, y, text, classes) {
    this.svg +=
        "<text" +
            xmlAttr("x", x) +
            xmlAttr("y", y) +
            xmlAttr("class", classes) +
        ">" +
            xmlEscape(text) +
        "</text>";
  };

  this.drawPath = function(points, classes, style) {
    var d_attr = "";
    var line_active = false;
    for (var i = 0; i < points.length; i++) {
      if (points[i][1] === null) {
        line_active = false;
        continue;
      }

      d_attr += line_active ? " L " : " M";
      line_active = true;
      d_attr += points[i][0] + " " + points[i][1];
    }

    this.svg +=
        "<path" +
            xmlAttr("class", classes) +
            xmlAttr("style", cssStringify(style)) +
            xmlAttr("d", d_attr) +
        " />";
  };

  this.drawPoint = function(x, y, point_size, classes) {
    this.svg +=
        "<circle" +
            xmlAttr("cx", x) +
            xmlAttr("cy", y) +
            xmlAttr("r", point_size) +
            xmlAttr("class", classes) +
        " />";
  };

  this.drawRect = function(x, y, width, height, classes) {
    this.svg +=
        "<rect" +
            xmlAttr("x", x) +
            xmlAttr("y", y) +
            xmlAttr("width", width) +
            xmlAttr("height", height) +
            xmlAttr("class", classes) +
        " />";
  };

  function xmlAttr(name, value) {
    return " " + name + "='" +  value + "'"; // FIXME WARNING: does not escape...
  }

  function xmlEscape(val) {
    // FIXME WARNING: does not escape...
    return val;
  }

  function cssStringify(style) {
    if (!style) {
      return "";
    }

    var css_stanzas = [];
    for (var k in style) {
      css_stanzas.push(k + ":" + style[k]);
    }

    return css_stanzas.join(";");
  }

};
