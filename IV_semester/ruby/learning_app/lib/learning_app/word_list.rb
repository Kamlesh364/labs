module LearningSystem
  class WordList
    attr_reader :words, :times_taken

    def initialize(name)
      raise "#{self.class} must have a name" if name.nil? or name =~ /^( )*$/

      @name = name
      @times_taken = 0
      @words = {}
    end

    def add_word(word)
      @words[word.to_sym] = word
    end

    def remove_word(word)
      @words.delete(word.to_sym)
    end

    def take
      @times_taken += 1
    end

    def to_sym
      @name.to_sym
    end
  end
end
