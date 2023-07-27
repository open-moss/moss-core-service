import os
import yaml

def load_config(config_path):
    with open(config_path, "r", encoding="utf-8") as f:
        data = f.read()
    config = yaml.load(data, yaml.Loader)
    return SpeakerConfig(**config)

class SpeakerConfig():
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            if type(v) == dict:
                v = SpeakerConfig(**v)
            self[k] = v
        if(self.model_path is None):
            raise ValueError("model_path is required")
        if(self.model_config_path is None):
            raise ValueError("model_config_path is required")
        if(self.speaker_id):
            raise ValueError("speaker_id is required")

    def keys(self):
        return self.__dict__.keys()

    def items(self):
        return self.__dict__.items()

    def values(self):
        return self.__dict__.values()

    def __len__(self):
        return len(self.__dict__)

    def __getitem__(self, key):
        return getattr(self, key)

    def __setitem__(self, key, value):
        return setattr(self, key, value)

    def __contains__(self, key):
        return key in self.__dict__

    def __repr__(self):
        return self.__dict__.__repr__()