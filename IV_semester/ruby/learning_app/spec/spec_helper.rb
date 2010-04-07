$: << File.join(File.dirname(__FILE__), "/../lib" )

require 'spec'
require 'learning_app'

# test - data
require 'spec/user_samples'
require 'spec/word_samples'

# matchers
#require 'spec/matchers/format_matcher'

#Spec::Runner.configure do |config|
#  config.include(LearningSystem::Matchers)
#end

Spec::Matchers.define :return_valid_format do
  match do |value|
    (value.to_s =~ /^[0-1]\.[0-9]([0-9])?$/) != nil
  end

  failure_message_for_should do |value|
    "expected #{value} to be floating point number between 0 and 1 with at most 2 decimal places"
  end

  failure_message_for_should do |value|
    "expected #{value} not to be floating point number between 0 and 1 with at most 2 decimal places"
  end
end
