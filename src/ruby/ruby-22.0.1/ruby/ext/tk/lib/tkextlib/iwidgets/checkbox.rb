#
#  tkextlib/iwidgets/checkbox.rb
#                               by Hidetoshi NAGAI (nagai@ai.kyutech.ac.jp)
#

require 'tk'
require 'tkextlib/iwidgets.rb'

module Tk
  module Iwidgets
    class  Checkbox < Tk::Iwidgets::Labeledframe
    end
  end
end

class Tk::Iwidgets::Checkbox
  TkCommandNames = ['::iwidgets::checkbox'.freeze].freeze
  WidgetClassName = 'Checkbox'.freeze
  WidgetClassNames[WidgetClassName] = self

  ####################################

  include TkItemConfigMethod

  def __item_cget_cmd(id)
    [self.path, 'buttoncget', id]
  end
  private :__item_cget_cmd

  def __item_config_cmd(id)
    [self.path, 'buttonconfigure', id]
  end
  private :__item_config_cmd

  def tagid(tagOrId)
    if tagOrId.kind_of?(Tk::Itk::Component)
      tagOrId.name
    else
      #_get_eval_string(tagOrId)
      tagOrId
    end
  end

  alias buttoncget itemcget
  alias buttonconfigure itemconfigure
  alias buttonconfiginfo itemconfiginfo
  alias current_buttonconfiginfo current_itemconfiginfo

  private :itemcget, :itemconfigure
  private :itemconfiginfo, :current_itemconfiginfo

  ####################################

  def add(tag=nil, keys={})
    if tag.kind_of?(Hash)
      keys = tag
      tag = nil
    end
    if tag
      tag = Tk::Itk::Component.new(self, tagid(tag))
    else
      tag = Tk::Itk::Component.new(self)
    end
    tk_call(@path, 'add', tagid(tag), *hash_kv(keys))
    tag
  end

  def delete(idx)
    tk_call(@path, 'delete', index(idx))
    self
  end

  def deselect(idx)
    tk_call(@path, 'deselect', index(idx))
    self
  end

  def flash(idx)
    tk_call(@path, 'flash', index(idx))
    self
  end

  def get(idx)
    simplelist(tk_call(@path, 'get', index(idx))).collect{|id|
      Tk::Itk::Component.id2obj(id)
    }
  end

  def index(idx)
    number(tk_call(@path, 'index', tagid(idx)))
  end

  def insert(idx, tag=nil, keys={})
    if tag.kind_of?(Hash)
      keys = tag
      tag = nil
    end
    if tag
      tag = Tk::Itk::Component.new(self, tagid(tag))
    else
      tag = Tk::Itk::Component.new(self)
    end
    tk_call(@path, 'insert', index(idx), tagid(tag), *hash_kv(keys))
    tag
  end

  def select(idx)
    tk_call(@path, 'select', index(idx))
    self
  end
end
