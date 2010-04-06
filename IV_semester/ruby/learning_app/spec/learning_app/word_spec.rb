require File.join(File.dirname(__FILE__), '/../spec_helper') 

module LearningSystem
  describe Word do
    include WordSamples

    context "creating" do
      before(:each) do
        @good_word_samples = good_word_samples
        @bad_word_samples = bad_word_samples
      end

      context "creating valid words" do
        it "should create valid words" do
          @good_word_samples.each do |sample|
            lambda { Word.new(sample[:value], sample[:translation], sample[:hint])}.should_not raise_error
          end
        end
      end

      context "creating invalid words" do
        it "should raise an exception when invalid parameters are specified" do
          @bad_word_samples.each do |sample|
            lambda { Word.new(sample[:value], sample[:translation], sample[:hint])}.should raise_error
          end
        end
      end
    end

    context "tagging" do
      before(:each) do
        @a = Word.new("Clojure", "functional programming language", "LISP dialect")
      end

      it "should be tagged" do
        tags = ["programming language", "lisp-like", "jvm"]
        @a.add_tags(tags)
        @a.should have(tags.size).tags
      end

      it "should not be tagged with illogic tags" do
        wrong_tags = [nil, ' ']

        @a.should have(0).tags
        @a.add_tags(wrong_tags)
        @a.should have(0).tags
      end

      it "should reference tag" do
        @a.add_tags(["sample tag"])
        @a.tag("sample tag").should_not be_nil
      end

      it "should strip preceding and succeeding white space" do
        tags = [' one ', ' two', 'three ']
        @a.add_tags(tags)

        tags.each do |t|
          @a.tag(t.strip).should_not be_nil 
          @a.tag(t).should_not be_nil 
        end
      end

      it "should have its tag removed" do
        tags = [' one ', ' two', 'three ']
        @a.add_tags(tags)

        tags.each do |t|
          @a.remove_tag(t)
        end

        @a.should have(0).tags
      end
    end

    context "guessing " do
      before(:each) do
        @word = Word.new("Test", "form of examination", "similar to quiz")
        @correct_value_guesses = ["Test", "TEST", "test", " test ", "test ", " test"]
        @correct_translation_guesses = ["FORM OF EXAMINATION ", "form of examination", "FORM of Examination", " form of examination "]
      end

      it "should be guessed by value" do
        @correct_value_guesses.each do |answer|
          @word.guess(:value => answer).should == true
        end
      end

      it "should be guessed by translation" do
        @correct_translation_guesses.each do |answer|
          @word.guess(:translation => answer).should == true
        end
      end

      it "should be guessed by both value and translation" do
        @correct_value_guesses.each do |answer|
          @word.guess( :value => answer, :translation => "quiz" ).should == false
        end

        @correct_translation_guesses.each do |answer|
          @word.guess( :value => "quiz", :translation => answer).should == false
        end

        @correct_value_guesses.zip(@correct_translation_guesses).each do |value, translation|
          @word.guess( :value => value, :translation => translation ).should == true
        end
      end

      it "should return false if guess was incorrect" do
        @word.guess( :value => "quiz" ).should == false
        @word.guess( :translation => "quiz" ).should == false
        @word.guess( :translation => "quiz", :value => "quiz" ).should == false
      end

      context "counting guessing statistics" do
        it "should be guessed and answerred equally" do
          @correct_value_guesses.each do |answer|
            @word.guess( :value => answer)
          end

          @word.times_guessed.should == @correct_value_guesses.size
          @word.times_answered.should == @correct_value_guesses.size 
        end

        it "should be guessed 3 times and answerred 0 times" do
          2.times { @word.guess( :value => "quiz") }
          @word.guess( :translation => "quiz")

          @word.times_guessed.should == 3
          @word.times_answered.should == 0
        end
      end
  end
end
end
